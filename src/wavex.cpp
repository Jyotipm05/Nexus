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
