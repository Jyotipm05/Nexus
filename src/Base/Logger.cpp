/**
 * @file Logger.cpp
 * @brief Logger implementation — currently header-only, this file ensures
 *        the translation unit exists for future non-inline additions.
 */

#include <wavex/Base/Logger.hpp>

// Logger is currently fully inline/header-implemented.
// This TU exists so the build system has a .cpp to compile,
// and for future non-template additions (e.g., async log queue).
