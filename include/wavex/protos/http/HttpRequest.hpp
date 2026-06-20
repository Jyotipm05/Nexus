/**
 * @file HttpRequest.hpp
 * @brief Concrete HTTP/1.x implementation of base::Request.
 *
 * Owns the received buffer; the parsed http1codec::request views point
 * into the owned buffer for zero-copy access.
 */

#pragma once

#include <string>
#include <wavex/Base/Request.hpp>
#include <wavex/Base/Url.hpp>
#include <wavex/protos/http/http1codec.hpp>

namespace wavex::protos::http {
    /**
     * @class HttpRequest
     * @brief HTTP/1.x request — owns buffer, exposes base::Request interface.
     */
    class HttpRequest final : public base::Request {
    public:
        /// Construct from a raw buffer. Parses and takes ownership.
        explicit HttpRequest(std::string buffer)
            : buffer_(std::move(buffer)) {
        }

        /// Parse the owned buffer. Returns true on success.
        bool parse() {
            size_t consumed = 0;
            if (const auto result = parser::parse_request(buffer_, parsed_, consumed);
                result != parser::result::success)
                return false;

            // Split target into path and query
            auto target = parsed_.target;
            if (const size_t q = target.find('?');
                q != std::string_view::npos) {
                // path_ = std::string(target.substr(0, q)); ##
                path_.assign(target, 0, q);
                query = url::parse_query(target.substr(q + 1));
            } else {
                // path_ = std::string(target); ##
                path_.assign(target);
            }

            return true;
        }

        /// The HTTP method
        [[nodiscard]] method method_type() const;

        /// The raw target (path + query string)
        [[nodiscard]] std::string_view target() const { return parsed_.target; }

        // base::Request interface
        [[nodiscard]] std::string_view path() const override { return path_; }

        [[nodiscard]] std::optional<std::string_view> header(const std::string_view name) const override {
            return parsed_.get_header(name);
        }

        [[nodiscard]] std::string_view body() const override { return parsed_.body; }

        /// Access the raw parsed codec request
        [[nodiscard]] const http::request &raw() const { return parsed_; }

    private:
        std::string buffer_; ///< owned receive buffer
        http::request parsed_; ///< zero-copy views into buffer_
        std::string path_; ///< extracted path (without query)
    };
} // namespace wavex::protos::http
