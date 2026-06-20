/**
 * @file Router.cpp
 * @brief Router implementation translation unit for WaveX.
 *
 * Explicitly instantiates Router<HttpProto> for the compiled WaveX library target.
 */

#include <wavex/Engine/Router.hpp>
#include <wavex/Engine/HttpRouter.hpp>

namespace wavex::engine {
    // Explicit template instantiation for the HTTP specialisation
    template class Router<HttpProto>;
}
