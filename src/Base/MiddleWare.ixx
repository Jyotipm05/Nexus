/**
 * @file MiddleWare.ixx
 * @brief C++ module interface for the WaveX middleware pipeline types.
 *
 * Exports wavex::base::Next and wavex::base::MiddlewareFn from the
 * wavex:middleware partition so they can be consumed via:
 *
 *   import wavex;           // transitively re-exports :middleware
 *   import wavex:middleware; // direct partition import
 */

module;

// Global module fragment — legacy headers included here are invisible
// outside this translation unit but their declarations are available
// to the module body below.
#define ASIO_HAS_CO_AWAIT 1
#include <functional>
#include <asio/awaitable.hpp>
#include <wavex/Base/Request.hpp>
#include <wavex/Base/Response.hpp>

export module wavex:middleware;

export namespace wavex::base {
    /// Callable that invokes the next middleware or the final handler.
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
} // export namespace wavex::base
