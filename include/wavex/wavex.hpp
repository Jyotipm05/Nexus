/**
 * @file wavex.hpp
 * @brief Main entry header for the WaveX framework.
 * 
 * This file declares core utility functions and macros, such as version information,
 * and includes essential protocol, routing, and server definitions.
 */

#pragma once

#include <wavex/protos/protos.hpp>
#include <wavex/protos/http/HttpResponse.hpp>
#include <wavex/Engine/Router.hpp>
#include <wavex/Engine/HttpRouter.hpp>
#include <wavex/Server/Server.hpp>

/**
 * @brief Macro representing the current version of the WaveX framework.
 */
#define wx_version "0.0.0"

/**
 * @namespace wavex
 * @brief The main namespace for all WaveX classes, functions, and symbols.
 */
namespace wavex {
    /**
     * @brief Prints the current version of the WaveX library/framework to standard output.
     */
    void _version();
}