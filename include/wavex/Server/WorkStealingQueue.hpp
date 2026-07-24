/**
 * @file WorkStealingQueue.hpp
 * @brief Tokio-style dual-queue system:
 *
 *   - LocalQueue   : Bounded, lock-free ring buffer (256 slots) per worker thread.
 *                    Owner pushes/pops from the back (LIFO, cache-local).
 *                    Thieves steal from the front (FIFO, work-stealing).
 *                    Uses Chase-Lev style atomic top/bottom indices.
 *
 *   - InjectorQueue: Unbounded, lock-based global MPMC queue.
 *                    Accepts tasks from any thread (external submitters, overflowed locals).
 *                    Workers drain it when their local queue is empty and stealing fails.
 */

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace wavex::server {
    using Task = std::function<void()>;

    // ─────────────────────────────────────────────────────────────────────────
    // LocalQueue — Bounded, lock-free ring buffer (256 slots)
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @class LocalQueue
     * @brief Per-worker bounded lock-free ring buffer with 256 task slots.
     *
     * Based on the Chase-Lev work-stealing deque:
     *   - The **owner** thread calls push() and pop() (back operations).
     *   - **Thief** threads call steal() (front operation).
     *
     * Capacity is fixed at exactly 256 (CAPACITY). push() returns false on
     * overflow — the caller must spill into the InjectorQueue.
     *
     * Memory ordering:
     *   - bottom_ is only written by the owner thread (relaxed write, release on publish).
     *   - top_ is read by all threads, written only by thieves under CAS.
     *   - The slot array uses seq_cst to prevent reordering of the write-then-bottom-publish.
     */
    class LocalQueue {
    public:
        static constexpr std::size_t CAPACITY = 256;

        LocalQueue() : top_(0), bottom_(0) {}

        LocalQueue(const LocalQueue &) = delete;
        LocalQueue &operator=(const LocalQueue &) = delete;

        /**
         * @brief Owner-only: push a task onto the back of the ring.
         * @return true if queued, false if the ring is full (spill to InjectorQueue).
         */
        [[nodiscard]] bool push(Task task) {
            const std::size_t b = bottom_.load(std::memory_order_relaxed);
            const std::size_t t = top_.load(std::memory_order_acquire);

            // Full check: ring has CAPACITY-1 usable slots to avoid ambiguity.
            if (b - t >= CAPACITY - 1) return false;

            slots_[b & MASK] = std::move(task);
            // Release so steal() readers see the fully-written task.
            bottom_.store(b + 1, std::memory_order_release);
            return true;
        }

        /**
         * @brief Owner-only: pop a task from the back (LIFO — cache-hot).
         * @return The task, or std::nullopt if empty.
         */
        std::optional<Task> pop() {
            std::size_t b = bottom_.load(std::memory_order_relaxed);
            if (b == 0) return std::nullopt;
            b -= 1;
            bottom_.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);

            std::size_t t = top_.load(std::memory_order_relaxed);
            if (t <= b) {
                if (t == b) {
                    // Last element in queue — race with a stealing thread.
                    if (!top_.compare_exchange_strong(t, t + 1,
                            std::memory_order_seq_cst, std::memory_order_relaxed)) {
                        // Lost race to a thief.
                        bottom_.store(b + 1, std::memory_order_relaxed);
                        return std::nullopt;
                    }
                    bottom_.store(b + 1, std::memory_order_relaxed);
                }
                // Won race (or t < b where thief operates on a different slot)
                Task task = std::move(slots_[b & MASK]);
                return task;
            }
            // Empty queue: restore bottom.
            bottom_.store(b + 1, std::memory_order_relaxed);
            return std::nullopt;
        }

        /**
         * @brief Thief-only: steal a task from the front (FIFO).
         * @return The stolen task, or std::nullopt if empty or lost race.
         */
        std::optional<Task> steal() {
            std::size_t t = top_.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            const std::size_t b = bottom_.load(std::memory_order_acquire);

            if (t >= b) return std::nullopt; // Empty.

            // CAS first: claim slot t atomically BEFORE moving content
            if (!top_.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return std::nullopt; // Lost race to another thief
            }
            Task task = std::move(slots_[t & MASK]);
            return task;
        }

        /// Approximate size (may race; used for load metrics only).
        [[nodiscard]] std::size_t size() const {
            const std::size_t b = bottom_.load(std::memory_order_relaxed);
            const std::size_t t = top_.load(std::memory_order_relaxed);
            return (b > t) ? (b - t) : 0;
        }

        [[nodiscard]] bool empty() const { return size() == 0; }

        /**
         * @brief Drain all remaining tasks (used only during worker retirement).
         * Called by the retiring owner thread after setting the stop flag,
         * so no concurrent steals occur.
         */
        std::vector<Task> drain_all() {
            std::vector<Task> drained;
            while (auto t = pop()) {
                drained.push_back(std::move(*t));
            }
            return drained;
        }

    private:
        static constexpr std::size_t MASK = CAPACITY - 1;
        static_assert((CAPACITY & MASK) == 0, "CAPACITY must be a power of 2");

        std::atomic<std::size_t> top_;
        // Padding to prevent false-sharing between top_ (thieves) and bottom_ (owner).
        char pad_[64 - sizeof(std::atomic<std::size_t>)]{};
        std::atomic<std::size_t> bottom_;
        std::array<Task, CAPACITY> slots_;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // InjectorQueue — Unbounded, lock-based global MPMC queue
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @class InjectorQueue
     * @brief Shared global task queue (the "injector" in Tokio terminology).
     *
     * Multiple producers and multiple consumers are supported under a single
     * std::mutex. This queue accepts:
     *   - Overflow tasks from LocalQueue::push() returning false.
     *   - Externally submitted tasks (e.g. ThreadPool::dispatch()).
     *
     * Workers drain this queue as a last resort when both their local ring
     * and work-stealing from peers have yielded nothing.
     */
    class InjectorQueue {
    public:
        InjectorQueue() = default;

        InjectorQueue(const InjectorQueue &) = delete;
        InjectorQueue &operator=(const InjectorQueue &) = delete;

        /// Push a task (any thread).
        void push(Task task) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(task));
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        /// Pop a task from the front (FIFO). Returns nullopt if empty.
        std::optional<Task> pop() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (head_ >= queue_.size()) {
                if (!queue_.empty()) {
                    queue_.clear();
                    head_ = 0;
                    size_.store(0, std::memory_order_relaxed);
                }
                return std::nullopt;
            }
            Task task = std::move(queue_[head_++]);
            size_.fetch_sub(1, std::memory_order_relaxed);
            if (head_ >= queue_.size()) {
                queue_.clear();
                head_ = 0;
                size_.store(0, std::memory_order_relaxed);
            }
            return task;
        }

        /// Lock-free size query
        [[nodiscard]] std::size_t size() const {
            return size_.load(std::memory_order_relaxed);
        }

        /// Lock-free empty check
        [[nodiscard]] bool empty() const {
            return size_.load(std::memory_order_relaxed) == 0;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<Task> queue_;
        std::size_t head_ = 0;
        std::atomic<std::size_t> size_{0};
    };

} // namespace wavex::server
