/**
 * @file HttpRouter.ixx
 * @brief C++ module interface for the HttpRouter component in WaveX.
 *
 * Exports wavex::engine::HttpProto and wavex::engine::HttpRouter from the
 * wavex:http_router partition so they can be consumed via:
 *
 *   import wavex;              // transitively re-exports :http_router
 *   import wavex:http_router;  // direct partition import
 */

module;

// Global module fragment
#include <wavex/Engine/HttpRouter.hpp>

export module wavex:http_router;

export namespace wavex::engine {
    using wavex::engine::HttpProto;
    using wavex::engine::HttpRouter;
}
