/**
 * @file Request.hpp
 * @brief Defines the Request base class/interface for handling incoming network requests.
 *
 * Provides base abstractions for HTTP/WS request parsing and state management.
 * Protocol-specific implementations (HttpRequest, etc.) derive from this.
 */

#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <unordered_map>

namespace wavex::base {
    /**
     * @class Request
     * @brief Protocol-agnostic request base class.
     *
     * Concrete protocol implementations (HTTP, MCP, etc.) override the pure
     * virtual accessors. The router populates `params` and `query` during
     * route resolution.
     */
    class Request {
    public:
        virtual ~Request() = default;

        /// The request path/target, e.g. "/user/123"
        [[nodiscard]] virtual std::string_view path() const = 0;

        /// Retrieve a header by name (case-insensitive for HTTP)
        [[nodiscard]] virtual std::optional<std::string_view> header(std::string_view name) const = 0;

        /// The request body
        [[nodiscard]] virtual std::string_view body() const = 0;

        /// Path parameters populated by the router (e.g. :id -> "123")
        std::unordered_map<std::string, std::string> params;

        /// Query string parameters (e.g. ?key=val -> {"key": "val"})
        std::unordered_map<std::string, std::string> query;
    };
} // namespace wavex::base