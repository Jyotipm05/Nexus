/**
 * @file Response.hpp
 * @brief Defines the Response base class/interface for representing outgoing network responses.
 *
 * Provides base abstractions for HTTP/WS response generation and header/body management.
 * Features a fluent API for method chaining: res.status(200).set("X-Foo", "bar").send("body").
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <ranges>
#include <cctype>
#include <nlohmann/json.hpp>

namespace wavex::base {
    /**
     * @class Response
     * @brief Protocol-agnostic CRTP response builder with fluent API (zero-vtable overhead).
     *
     * Concrete protocol implementations inherit via CRTP:
     * `class HttpResponse final : public base::Response<HttpResponse>`.
     */
    template <typename Derived>
    class Response {
    public:
        ~Response() = default;

        /// Set the response status code
        Derived &status(const unsigned int code) {
            status_code_ = code;
            return static_cast<Derived &>(*this);
        }

        /// Set a response header (appends; does not deduplicate)
        Derived &set(const std::string_view name, const std::string_view value) {
            headers_.emplace_back(std::string(name), std::string(value));
            return static_cast<Derived &>(*this);
        }

        /// Returns true if the response has already been sent
        [[nodiscard]] bool is_sent() const { return is_sent_; }

        /// Set the response body as plain text and mark as sent
        Derived &send(const std::string_view body) {
            return static_cast<Derived *>(this)->send_impl(body);
        }

        /// Set the response body as JSON — sets Content-Type automatically
        Derived &json(const nlohmann::json &j) {
            set("Content-Type", "application/json");
            return send(j.dump());
        }

        /// Serialize the response into the wire format (protocol-specific)
        [[nodiscard]] std::string serialize() const {
            return static_cast<const Derived *>(this)->serialize_impl();
        }

        /// Access the current status code
        [[nodiscard]] unsigned int status_code() const { return status_code_; }

        /// Access the current body
        [[nodiscard]] const std::string &get_body() const { return body_; }

        /// Check if a header exists (case-insensitive for HTTP)
        [[nodiscard]] bool has_header(const std::string_view name) const {
            for (const auto &k: headers_ | std::views::keys) {
                if (k.size() == name.size()) {
                    bool match = true;
                    for (size_t i = 0; i < k.size(); ++i) {
                        if (std::tolower(static_cast<unsigned char>(k[i])) !=
                            std::tolower(static_cast<unsigned char>(name[i]))) {
                            match = false;
                            break;
                        }
                    }
                    if (match) return true;
                }
            }
            return false;
        }

    protected:
        unsigned int status_code_ = 200;
        std::vector<std::pair<std::string, std::string> > headers_;
        std::string body_;
        bool is_sent_ = false;
    };
} // namespace wavex::base
