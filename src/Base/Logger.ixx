// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/**
 * @file Logger.ixx
 * @brief C++ module interface for WaveX Logger and modern wavex::log API.
 */

module;

#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <mutex>
#include <optional>
#include <format>
#include <chrono>
#include <thread>
#include <source_location>
#include <cstdlib>
#include <filesystem>
#include <type_traits>
#include <wavex/Base/Logger.hpp>

export module wavex:logger;

export namespace wavex::base {
    using wavex::base::LogLevel;
    using wavex::base::log_level_tag;
    using wavex::base::log_level_color;
    using wavex::base::Logger;
}

export namespace wavex::log {
    using wavex::log::format_with_loc;
    using wavex::log::msg_with_loc;
    using wavex::log::trace;
    using wavex::log::debug;
    using wavex::log::info;
    using wavex::log::warn;
    using wavex::log::error;
    using wavex::log::fatal;
}
