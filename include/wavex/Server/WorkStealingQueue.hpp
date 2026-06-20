/**
 * @file WorkStealingQueue.hpp
 * @brief Thread-safe task queue supporting local push/pop, work-stealing, and queue draining.
 */

#pragma once

#include <deque>
#include <mutex>
#include <functional>
#include <vector>
#include <optional>

namespace wavex::server {
    using Task = std::function<void()>;

    /**
     * @class WorkStealingQueue
     * @brief Thread-safe queue for worker task scheduling, work-stealing, and scale-down task draining.
     */
    class WorkStealingQueue {
    public:
        WorkStealingQueue() = default;

        ~WorkStealingQueue() = default;

        WorkStealingQueue(const WorkStealingQueue &) = delete;

        WorkStealingQueue &operator=(const WorkStealingQueue &) = delete;

        /// Push a task to the back of the queue (owner thread)
        void push(Task task) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(task));
        }

        /// Pop a task from the back of the queue (owner thread, LIFO for cache locality)
        std::optional<Task> pop() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) return std::nullopt;
            Task task = std::move(queue_.back());
            queue_.pop_back();
            return task;
        }

        /// Steal a task from the front of the queue (thief worker thread, FIFO order)
        std::optional<Task> steal() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) return std::nullopt;
            Task task = std::move(queue_.front());
            queue_.pop_front();
            return task;
        }

        /// Drain all remaining queued tasks (used during worker decommission to redistribute workload)
        std::vector<Task> drain_all() {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<Task> drained;
            drained.reserve(queue_.size());
            while (!queue_.empty()) {
                drained.push_back(std::move(queue_.front()));
                queue_.pop_front();
            }
            return drained;
        }

        /// Check current queue depth
        [[nodiscard]] std::size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

        /// Check if the queue is empty
        [[nodiscard]] bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

    private:
        mutable std::mutex mutex_;
        std::deque<Task> queue_;
    };
} // namespace wavex::server
