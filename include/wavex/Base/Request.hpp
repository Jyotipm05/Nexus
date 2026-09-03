// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
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
     * @brief Protocol-agnostic CRTP request base class (zero-vtable overhead).
     *
     * Concrete protocol implementations (HttpRequest, etc.) inherit via CRTP:
     * `class HttpRequest final : public base::Request<HttpRequest>`.
     */
    template <typename Derived>
    class Request {
    public:
        ~Request() = default;

        /// The request path/target, e.g. "/user/123"
        [[nodiscard]] std::string_view path() const {
            return static_cast<const Derived *>(this)->path_impl();
        }

        /// Retrieve a header by name (case-insensitive for HTTP)
        [[nodiscard]] std::optional<std::string_view> header(const std::string_view name) const {
            return static_cast<const Derived *>(this)->header_impl(name);
        }

        /// The request body
        [[nodiscard]] std::string_view body() const {
            return static_cast<const Derived *>(this)->body_impl();
        }

        /// Path parameters populated by the router (e.g. :id -> "123")
        std::unordered_map<std::string, std::string> params;

        /// Query string parameters (e.g. ?key=val -> {"key": "val"})
        std::unordered_map<std::string, std::string> query;
    };
} // namespace wavex::base