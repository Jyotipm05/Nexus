/**
 * @file HttpResponse.hpp
 * @brief Concrete HTTP/1.x implementation of base::Response.
 *
 * Inherits from base::Response and implements serialize() using the http1codec encoder.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <wavex/Base/Response.hpp>
#include <wavex/protos/http/http1codec.hpp>

namespace wavex::protos::http {
    /**
     * @class HttpResponse
     * @brief HTTP implementation of base::Response with fluent API.
     */
    class HttpResponse final : public base::Response {
    public:
        HttpResponse() = default;
        explicit HttpResponse(asio::ip::tcp::socket *socket) : socket_(socket) {}

        ~HttpResponse() override = default;

        /// Attach or update the target socket for immediate response writing
        void set_socket(asio::ip::tcp::socket *socket) { socket_ = socket; }

        /// Get stored socket pointer
        [[nodiscard]] asio::ip::tcp::socket *socket() const { return socket_; }

        /// Override send to immediately write serialized HTTP response to the socket if bound
        Response &send(const std::string_view body) override {
            if (is_sent_) return *this;
            body_ = std::string(body);
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
            res.status_text = status_text_for(status_code_);
            res.body = body_;

            res.headers.reserve(headers_.size());
            for (const auto &[name, value]: headers_) {
                res.headers.push_back(http::header{name, value});
            }

            return http::encoder::serialize(res);
        }

    private:
        asio::ip::tcp::socket *socket_ = nullptr;
    };
} // namespace wavex::protos::http
