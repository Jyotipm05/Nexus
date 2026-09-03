// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Logger.cpp
 * @brief Logger implementation — currently header-only, this file ensures
 *        the translation unit exists for future non-inline additions.
 */

#include <wavex/Base/Logger.hpp>

// Logger is currently fully inline/header-implemented.
// This TU exists so the build system has a .cpp to compile,
// and for future non-template additions (e.g., async log queue).
