/**
 * @file HttpResponse.hpp
 * @brief Concrete HTTP/1.x implementation of base::Response.
 *
 * Supports both server-side response creation & immediate socket writing,
 * and zero-copy client-side response parsing & inspection (for 3rd-party services).
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <wavex/Base/Response.hpp>
#include <wavex/protos/http/http1codec.hpp>

namespace wavex::protos::http {
    /**
     * @class HttpResponse
     * @brief HTTP implementation of base::Response with fluent API and zero-copy view storage.
     */
    class HttpResponse final : public base::Response {
    public:
        HttpResponse() = default;

        explicit HttpResponse(asio::ip::tcp::socket *socket) : socket_(socket) {}

        ~HttpResponse() override = default;

        /**
         * @brief Copy constructor — deep-copies backing stores and rebases all
         *        string_view members so they point into THIS object's buffers.
         *
         * The default copy constructor deep-copies the std::string owners
         * (buffer_owner_, dechunked_body_storage_, body_) but leaves every
         * string_view pointing at the SOURCE's memory — a use-after-free
         * once the source is destroyed.  This is the root cause of garbled
         * header names / missing header lookups when HttpClient coroutines
         * copy the response through co_return.
         */
        HttpResponse(const HttpResponse &other)
            : base::Response(other),
              socket_(other.socket_),
              buffer_owner_(other.buffer_owner_),
              dechunked_body_storage_(other.dechunked_body_storage_),
              parsed_(other.parsed_) {
            // Rebase helper: given a string_view that pointed into
            // other.buffer_owner_, return an equivalent view into this->buffer_owner_.
            const auto buf_base = other.buffer_owner_.data();
            const auto buf_len  = other.buffer_owner_.size();
            const auto my_base  = buffer_owner_.data();
            const auto my_len   = buffer_owner_.size();

            auto rebase_buf = [buf_base, buf_len, my_base, my_len](
                const std::string_view sv) -> std::string_view {
                if (sv.data() == nullptr) return {};
                const auto off = static_cast<std::size_t>(sv.data() - buf_base);
                if (off < buf_len && off + sv.size() <= buf_len)
                    return {my_base + off, sv.size()};
                return sv;
            };

            // Rebase status_text_ and parsed_ views (all from buffer_owner_)
            if (buf_len > 0) {
                status_text_ = rebase_buf(other.status_text_);
                parsed_.status_text = rebase_buf(other.parsed_.status_text);
                parsed_.body = rebase_buf(other.parsed_.body);
                for (auto &h : parsed_.headers) {
                    h.name  = rebase_buf(h.name);
                    h.value = rebase_buf(h.value);
                }
            }

            // Rebase body_view_ — may point into dechunked_body_storage_
            if (!other.dechunked_body_storage_.empty()) {
                const auto dk_base = other.dechunked_body_storage_.data();
                const auto dk_len  = other.dechunked_body_storage_.size();
                const auto off = static_cast<std::size_t>(
                    other.body_view_.data() - dk_base);
                if (off < dk_len && off + other.body_view_.size() <= dk_len)
                    body_view_ = {dechunked_body_storage_.data() + off,
                                  other.body_view_.size()};
            }

            // Rebase headers_views_ (all from buffer_owner_)
            headers_views_.reserve(other.headers_views_.size());
            if (buf_len > 0) {
                for (const auto &[k, v] : other.headers_views_)
                    headers_views_.emplace_back(rebase_buf(k), rebase_buf(v));
            } else {
                headers_views_ = other.headers_views_;
            }
        }

        /// Attach or update the target socket for immediate response writing
        void set_socket(asio::ip::tcp::socket *socket) { socket_ = socket; }

        /// Get stored socket pointer
        [[nodiscard]] asio::ip::tcp::socket *socket() const { return socket_; }

        /**
         * @brief Zero-copy parse a raw HTTP response buffer into this object (used by HttpClient).
         * Storage is safely owned by buffer_owner_; string_views point into owned storage.
         * @param buffer Raw network response string.
         * @return true on successful parsing.
         */
        bool parse(const std::string_view buffer) {
            buffer_owner_ = std::string(buffer);
            parsed_ = {};
            headers_views_.clear();

            std::size_t consumed = 0;
            if (parser::parse_response(buffer_owner_, parsed_, consumed) != parser::result::success) {
                headers_views_.clear();
                return false;
            }

            status_code_ = parsed_.status_code;
            status_text_ = parsed_.status_text;

            // Handle chunked response un-chunking
            if (const auto te = parsed_.get_header("Transfer-Encoding");
                te && te->find("chunked") != std::string_view::npos) {
                dechunked_body_storage_ = decoder::dechunk(parsed_.body);
                body_view_ = dechunked_body_storage_;
            } else {
                body_view_ = parsed_.body;
            }
            body_ = std::string(body_view_);

            headers_views_.clear();
            headers_views_.reserve(parsed_.headers.size());
            for (const auto &[name, value]: parsed_.headers) {
                headers_views_.emplace_back(name, value);
            }
            return true;
        }

        /// Override send to immediately write serialized HTTP response to the socket if bound
        Response &send(const std::string_view body) override {
            if (is_sent_) return *this;
            body_ = std::string(body);
            body_view_ = body_;
            is_sent_ = true;
            if (socket_) {
                std::string serialized = serialize();
                asio::write(*socket_, asio::buffer(serialized));
            }
            return *this;
        }

        /**
         * @brief Serialize the HTTP response into wire format (status line + headers + body).
         * @return Serialized HTTP response string ready to be transmitted over the socket.
         */
        [[nodiscard]] std::string serialize() const override {
            http::response res;
            res.status_code = status_code_;
            res.status_text = status_text_;
            res.body = body_view_.empty() ? std::string_view(body_) : body_view_;

            if (!headers_views_.empty()) {
                res.headers.reserve(headers_views_.size());
                for (const auto &[name, value]: headers_views_) {
                    res.headers.emplace_back(name, value);
                }
            } else {
                res.headers.reserve(headers_.size());
                for (const auto &[name, value]: headers_) {
                    res.headers.emplace_back(name, value);
                }
            }

            return http::encoder::serialize(res);
        }

        /// Access status text as zero-copy std::string_view
        [[nodiscard]] std::string_view status_text() const { return status_text_; }

        /// Retrieve a header value by name (zero-copy std::string_view)
        [[nodiscard]] std::optional<std::string_view> header(const std::string_view name) const {
            if (!headers_views_.empty()) {
                for (const auto &[k, v]: headers_views_) {
                    if (detail::is_equal(k, name)) return v;
                }
            } else {
                for (const auto &[k, v]: headers_) {
                    if (detail::is_equal(k, name)) return v;
                }
            }
            return std::nullopt;
        }

        /// Access zero-copy headers views
        [[nodiscard]] const std::vector<std::pair<std::string_view, std::string_view>> &header_views() const {
            return headers_views_;
        }

        /// Access the raw unparsed HTTP response wire buffer
        [[nodiscard]] std::string_view raw_response() const { return buffer_owner_; }

    private:
        asio::ip::tcp::socket *socket_ = nullptr;
        std::string_view status_text_ = "OK";
        std::string buffer_owner_;
        std::string dechunked_body_storage_;
        std::string_view body_view_;
        http::response parsed_;
        std::vector<std::pair<std::string_view, std::string_view>> headers_views_;
    };
} // namespace wavex::protos::http
