/**
 * @file ThreadPool.ixx
 * @brief C++ module interface partition for ThreadPool in WaveX.
 */

module;

#include <wavex/Server/ThreadPool.hpp>

export module wavex:server_pool;

export namespace wavex::server {
    using wavex::server::ThreadPoolConfig;
    using wavex::server::WorkerNode;
    using wavex::server::ThreadPool;
}
