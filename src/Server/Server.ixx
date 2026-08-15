/**
 * @file Server.ixx
 * @brief Primary C++ module interface partition for Server in WaveX.
 */

module;

#include <wavex/Server/WorkStealingQueue.hpp>
#include <wavex/Server/ThreadPool.hpp>
#include <wavex/Server/Server.hpp>

export module wavex:server;

export import :server_queue;
export import :server_pool;

export namespace wavex::server {
    using wavex::server::Task;
    using wavex::server::LocalQueue;
    using wavex::server::InjectorQueue;
    using wavex::server::ThreadPoolConfig;
    using wavex::server::WorkerNode;
    using wavex::server::ThreadPool;
    using wavex::server::Server;
    using wavex::server::Http1Server;
    using wavex::server::http1server;
}
