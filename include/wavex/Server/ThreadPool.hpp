/**
 * @file ThreadPool.hpp
 * @brief Tokio-like adaptive work-stealing thread pool with hysteresis scaling and task redistribution.
 */

#pragma once

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <mutex>
#include <asio/io_context.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <wavex/Server/WorkStealingQueue.hpp>

namespace wavex::server {
    /**
     * @struct ThreadPoolConfig
     * @brief Singleton configuration structure for thread pool limits and scaling thresholds.
     */
    class ThreadPoolConfig {
    public:
        static ThreadPoolConfig &instance() {
            static ThreadPoolConfig s_config;
            return s_config;
        }

        std::size_t min_workers = 1;
        std::size_t max_workers = 5;

        /// Upper load thresholds to trigger scale-up when running at N threads (index N-1)
        std::vector<std::size_t> upper_thresholds = {10, 25, 50, 100};

        /// Lower load thresholds to trigger scale-down when running at N threads (index N-2)
        std::vector<std::size_t> lower_thresholds = {5, 15, 30, 60};

        /// Interval between hysteresis scaling evaluations
        std::chrono::milliseconds check_interval{100};

        void set_limits(std::size_t min_w, std::size_t max_w) {
            min_workers = min_w;
            max_workers = max_w;
            if (upper_thresholds.size() < max_workers - 1) {
                upper_thresholds.resize(max_workers - 1, 50);
            }
            if (lower_thresholds.size() < max_workers - 1) {
                lower_thresholds.resize(max_workers - 1, 10);
            }
        }
    };

    /**
     * @struct WorkerNode
     * @brief Context and state for a single slave/worker thread in the pool.
     */
    struct WorkerNode {
        std::size_t id = 0;
        std::shared_ptr<asio::io_context> io_ctx;
        std::unique_ptr<WorkStealingQueue> queue;
        std::atomic<bool> is_retiring{false};
        std::atomic<bool> is_busy{false};
        std::atomic<bool> stop_requested{false};
        std::thread thread;

        WorkerNode(std::size_t worker_id)
            : id(worker_id),
              io_ctx(std::make_shared<asio::io_context>()),
              queue(std::make_unique<WorkStealingQueue>()) {
        }
    };

    /**
     * @class ThreadPool
     * @brief Adaptive Tokio-like work-stealing thread pool with threshold-based scaling.
     */
    class ThreadPool {
    public:
        explicit ThreadPool(ThreadPoolConfig &config = ThreadPoolConfig::instance())
            : config_(config) {
            start_pool();
        }

        ~ThreadPool() {
            stop_pool();
        }

        ThreadPool(const ThreadPool &) = delete;

        ThreadPool &operator=(const ThreadPool &) = delete;

        /// Dispatch a generic task to the worker pool
        void dispatch(Task task) {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            if (workers_.empty()) return;
            // Balance insertion across workers
            std::size_t idx = next_worker_idx_++ % workers_.size();
            workers_[idx]->queue->push(std::move(task));
            workers_[idx]->io_ctx->post([w = workers_[idx].get()] {
                w->queue->pop();
            });
        }

        /// Spawn an Asio coroutine onto one of the worker io_contexts
        template<typename Coro>
        void spawn_coroutine(Coro coro) {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            if (workers_.empty()) return;
            std::size_t idx = next_worker_idx_++ % workers_.size();
            asio::co_spawn(*workers_[idx]->io_ctx, std::move(coro), asio::detached);
        }

        /// Get current active worker thread count
        [[nodiscard]] std::size_t worker_count() const {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            return workers_.size();
        }

        /// Force an immediate scaling evaluation check
        void evaluate_scaling() {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            check_and_scale();
        }

    private:
        ThreadPoolConfig &config_;
        mutable std::mutex workers_mutex_;
        std::vector<std::shared_ptr<WorkerNode> > workers_;
        std::atomic<bool> pool_stopping_{false};
        std::thread monitor_thread_;
        std::atomic<std::size_t> next_worker_idx_{0};

        void start_pool() {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            for (std::size_t i = 0; i < config_.min_workers; ++i) {
                add_worker_unlocked();
            }
            pool_stopping_ = false;
            monitor_thread_ = std::thread([this] { monitor_loop(); });
        }

        void stop_pool() {
            pool_stopping_ = true;
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }

            std::lock_guard<std::mutex> lock(workers_mutex_);
            for (auto &w: workers_) {
                w->stop_requested = true;
                if (w->io_ctx) w->io_ctx->stop();
                if (w->thread.joinable()) w->thread.join();
            }
            workers_.clear();
        }

        void add_worker_unlocked() {
            std::size_t new_id = workers_.size() + 1;
            auto worker = std::make_shared<WorkerNode>(new_id);
            worker->thread = std::thread([this, w = worker] { worker_loop(w); });
            workers_.push_back(worker);
        }

        void worker_loop(const std::shared_ptr<WorkerNode> &w) {
            while (!w->stop_requested && !w->is_retiring) {
                // 1. Process local task queue
                if (auto task = w->queue->pop()) {
                    w->is_busy = true;
                    (*task)();
                    w->is_busy = false;
                    continue;
                }

                // 2. Work stealing from neighboring workers if local queue is empty
                bool stole_work = false; {
                    std::lock_guard<std::mutex> lock(workers_mutex_);
                    for (const auto &other: workers_) {
                        if (other->id != w->id && !other->queue->empty()) {
                            if (auto stolen = other->queue->steal()) {
                                w->is_busy = true;
                                (*stolen)();
                                w->is_busy = false;
                                stole_work = true;
                                break;
                            }
                        }
                    }
                }

                if (stole_work) continue;

                // 3. Poll io_context for event handlers / coroutines
                w->io_ctx->poll();

                // Short sleep if idle to prevent tight CPU spin
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        void monitor_loop() {
            while (!pool_stopping_) {
                std::this_thread::sleep_for(config_.check_interval);
                if (pool_stopping_) break;

                std::lock_guard<std::mutex> lock(workers_mutex_);
                check_and_scale();
            }
        }

        void check_and_scale() {
            if (workers_.empty()) return;

            std::size_t total_queue_depth = 0;
            std::size_t active_busy = 0;

            for (const auto &w: workers_) {
                total_queue_depth += w->queue->size();
                if (w->is_busy) ++active_busy;
            }

            std::size_t aggregate_load = total_queue_depth + active_busy;
            std::size_t current_count = workers_.size();

            // 1. Scale UP check
            if (current_count < config_.max_workers) {
                std::size_t thresh_idx = current_count - 1;
                std::size_t high_thresh = (thresh_idx < config_.upper_thresholds.size())
                                              ? config_.upper_thresholds[thresh_idx]
                                              : 50;
                if (aggregate_load > high_thresh) {
                    add_worker_unlocked();
                    return;
                }
            }

            // 2. Scale DOWN check (with graceful decommissioning & task redistribution)
            if (current_count > config_.min_workers) {
                std::size_t thresh_idx = current_count - 2;
                std::size_t low_thresh = (thresh_idx < config_.lower_thresholds.size())
                                             ? config_.lower_thresholds[thresh_idx]
                                             : 5;
                if (aggregate_load < low_thresh) {
                    decommission_worker_unlocked();
                }
            }
        }

        void decommission_worker_unlocked() {
            if (workers_.size() <= config_.min_workers) return;

            auto retiring_worker = workers_.back();
            workers_.pop_back();

            retiring_worker->is_retiring = true;

            // Wait for current active task coroutine to finish
            while (retiring_worker->is_busy) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            // Drain remaining queued tasks from retiring worker
            auto remaining_tasks = retiring_worker->queue->drain_all();

            // Redistribute drained tasks across surviving active workers
            if (!remaining_tasks.empty() && !workers_.empty()) {
                std::size_t idx = 0;
                for (auto &task: remaining_tasks) {
                    workers_[idx % workers_.size()]->queue->push(std::move(task));
                    ++idx;
                }
            }

            retiring_worker->stop_requested = true;
            if (retiring_worker->io_ctx) retiring_worker->io_ctx->stop();
            if (retiring_worker->thread.joinable()) {
                retiring_worker->thread.join();
            }
        }
    };
} // namespace wavex::server
