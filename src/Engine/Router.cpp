// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Router.cpp
 * @brief Router implementation translation unit for WaveX.
 *
 * Explicitly instantiates Router<HttpProto> for the compiled WaveX library target.
 */

#include <wavex/Engine/Router.hpp>
#include <wavex/Engine/HttpRouter.hpp>

namespace wavex::engine {
    // Explicit template instantiation for the default HTTP/1.x specialization
    template class Router<Http1Proto>;
}
