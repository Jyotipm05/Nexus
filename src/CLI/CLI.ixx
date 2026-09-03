// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Cli.ixx
 * @brief C++ module interface partition for built-in CLI argument parsing in WaveX.
 */

module;

#include <wavex/Cli/Cli.hpp>

export module wavex:cli;

export namespace wavex::cli {
    using cli::OptionType;
    using cli::Option;
    using cli::ParseResult;
    using cli::CliParser;
}
