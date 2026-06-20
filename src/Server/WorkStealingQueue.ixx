/**
 * @file WorkStealingQueue.ixx
 * @brief C++ module interface partition for WorkStealingQueue in WaveX.
 */

module;

#include <wavex/Server/WorkStealingQueue.hpp>

export module wavex:server_queue;

export namespace wavex::server {
    using wavex::server::Task;
    using wavex::server::WorkStealingQueue;
}
