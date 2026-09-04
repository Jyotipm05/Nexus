// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Server.ixx
 * @brief Primary C++ module interface partition for Server in WaveX.
 */

module;

#include <wavex/Server/TlsConfig.hpp>
#include <wavex/Server/WorkStealingQueue.hpp>
#include <wavex/Server/ThreadPool.hpp>
#include <wavex/Server/Server.hpp>

export module wavex:server;

export import :server_queue;
export import :server_pool;

export namespace wavex::server {
    using wavex::server::TlsConfig;
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
