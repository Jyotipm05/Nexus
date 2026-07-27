/**
 * @file wavex.ixx
 * @brief Primary C++ module interface file for the WaveX library.
 * 
 * Exports the main WaveX namespace symbols, functions, and submodule partitions.
 */

module;

#include <wavex/wavex.hpp>

export module wavex;
export import :protos;
export import :protos_http;
export import :middleware;
export import :router;
export import :http_router;
export import :server;
export import :client;

export namespace wavex {
    using wavex::_version;
}
