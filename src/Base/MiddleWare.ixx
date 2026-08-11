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
     * @brief Generic middleware function signature for CRTP Request and Response types.
     */
    template <typename ReqT, typename ResT>
    using GenericMiddlewareFn = std::function<asio::awaitable<void>(ReqT &, ResT &, Next)>;
} // export namespace wavex::base
