// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file MiddleWare.hpp
 * @brief Defines the middleware function type for the WaveX pipeline.
 *
 * A middleware is a coroutine-aware function that receives a Request, Response,
 * and a Next callable. Calling co_await next() passes control to the next
 * middleware in the chain (or the final route handler). Not calling next()
 * short-circuits the pipeline (e.g., for auth rejection).
 */

#pragma once

#include <functional>

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <asio/awaitable.hpp>

// Forward declarations of CRTP base templates
namespace wavex::base {
    template <typename Derived>
    class Request;

    template <typename Derived>
    class Response;

    /// Callable that invokes the next middleware or the final handler
    using Next = std::function<asio::awaitable<void>()>;

    /**
     * @brief Generic middleware function signature for CRTP Request and Response types.
     *
     * Usage:
     *   auto logger = [](ReqT &req, ResT &res, Next next) -> asio::awaitable<void> {
     *       WX_LOG_INFO("-> {}", req.path());
     *       co_await next();
     *       WX_LOG_INFO("<- {} {}", res.status_code(), req.path());
     *   };
     */
    template <typename ReqT, typename ResT>
    using GenericMiddlewareFn = std::function<asio::awaitable<void>(ReqT &, ResT &, Next)>;
} // namespace wavex::base
