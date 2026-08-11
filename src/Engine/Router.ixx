/**
 * @file Router.ixx
 * @brief C++ module interface file for the Router component in WaveX.
 *
 * Exports wavex::engine::Handler, RouteMatch, ScopedMiddleware, and Router
 * from the wavex:router partition.
 */

module;

// Global module fragment
#include <wavex/Engine/Router.hpp>

export module wavex:router;

export namespace wavex::engine {
    using wavex::engine::Router;
}
