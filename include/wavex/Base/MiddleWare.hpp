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

// Forward declarations to avoid circular includes
namespace wavex::base {
    class Request;
    class Response;
}

namespace wavex::base {
    /// Callable that invokes the next middleware or the final handler
    using Next = std::function<asio::awaitable<void>()>;

    /**
     * @brief Middleware function signature.
     *
     * Usage:
     *   auto logger = [](Request& req, Response& res, Next next) -> asio::awaitable<void> {
     *       WX_LOG_INFO("-> {}", req.path());
     *       co_await next();
     *       WX_LOG_INFO("<- {} {}", res.status_code(), req.path());
     *   };
     */
    using MiddlewareFn = std::function<asio::awaitable<void>(Request &, Response &, Next)>;
} // namespace wavex::base
