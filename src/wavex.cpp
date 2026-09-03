// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file wavex.cpp
 * @brief Implementation of core version and utility functions for WaveX.
 */

#include <print>
#include <wavex/wavex.hpp>

namespace wavex {
    /**
     * @brief Prints the current version of the WaveX library/framework to standard output.
     */
    void _version() {
        std::print("WaveX 2026 version {}\n", wx_version);
    }
}
