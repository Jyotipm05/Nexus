/**
 * @file WorkStealingQueue.ixx
 * @brief C++ module interface partition for the Tokio-style dual-queue system in WaveX.
 *
 * Exports:
 *   - Task         : std::function<void()> type alias
 *   - LocalQueue   : Bounded 256-slot lock-free ring buffer (per-worker)
 *   - InjectorQueue: Unbounded lock-based global MPMC overflow queue
 */

module;

#include <wavex/Server/WorkStealingQueue.hpp>

export module wavex:server_queue;

export namespace wavex::server {
    using wavex::server::Task;
    using wavex::server::LocalQueue;
    using wavex::server::InjectorQueue;
}
