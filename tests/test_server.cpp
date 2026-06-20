/**
 * @file test_server.cpp
 * @brief Unit & integration tests for HttpResponse, WorkStealingQueue, ThreadPool, and Server.
 */

#include <wavex/protos/http/HttpResponse.hpp>
#include <wavex/Server/WorkStealingQueue.hpp>
#include <wavex/Server/ThreadPool.hpp>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

namespace {
    int tests_run = 0;
    int tests_passed = 0;

    void check(const bool condition, const char *name) {
        ++tests_run;
        if (condition) {
            ++tests_passed;
            std::cout << "  [PASS] " << name << "\n";
        } else {
            std::cout << "  [FAIL] " << name << "\n";
        }
    }
}

// ─── Test 1: HttpResponse Serialization ───────────────────────────────────────

void test_http_response_serialization() {
    std::cout << "\n[Test 1] HttpResponse serialization & fluent API\n";

    wavex::protos::http::HttpResponse res;
    res.status(200).set("X-Custom-Header", "WaveXValue").send("Hello Server");

    check(res.status_code() == 200, "Status code is 200");
    check(res.get_body() == "Hello Server", "Body equals 'Hello Server'");
    check(res.has_header("X-Custom-Header"), "Has header 'X-Custom-Header'");

    std::string wire = res.serialize();
    check(wire.starts_with("HTTP/1.1 200 OK\r\n"), "Serialized status line matches HTTP/1.1 200 OK");
    check(wire.find("X-Custom-Header: WaveXValue\r\n") != std::string::npos, "Serialized output contains header");
    check(wire.find("Content-Length: 12\r\n") != std::string::npos, "Serialized output contains Content-Length: 12");
    check(wire.ends_with("Hello Server"), "Serialized wire ends with body 'Hello Server'");

    wavex::protos::http::HttpResponse json_res;
    nlohmann::json j = {{"status", "ok"}, {"count", 42}};
    json_res.status(201).json(j);

    std::string json_wire = json_res.serialize();
    check(json_res.status_code() == 201, "JSON response status is 201");
    check(json_wire.find("Content-Type: application/json\r\n") != std::string::npos, "JSON header set automatically");
}

// ─── Test 2: WorkStealingQueue Operations ────────────────────────────────────

void test_work_stealing_queue() {
    std::cout << "\n[Test 2] WorkStealingQueue push, pop, steal, and drain_all\n";

    wavex::server::WorkStealingQueue q;
    int counter = 0;

    q.push([&counter] { counter += 10; });
    q.push([&counter] { counter += 20; });
    q.push([&counter] { counter += 30; });

    check(q.size() == 3, "Queue size is 3");

    // Pop (LIFO order by owner thread)
    auto task1 = q.pop();
    check(task1.has_value(), "Task popped by owner");
    if (task1) (*task1)();
    check(counter == 30, "LIFO task 3 executed (+30)");

    // Steal (FIFO order by thief thread)
    auto stolen_task = q.steal();
    check(stolen_task.has_value(), "Task stolen by thief");
    if (stolen_task) (*stolen_task)();
    check(counter == 40, "FIFO stolen task 1 executed (+10)");

    // Drain all remaining
    q.push([&counter] { counter += 100; });
    auto drained = q.drain_all();
    check(q.empty(), "Queue is empty after drain_all()");
    check(drained.size() == 2, "Drained 2 remaining tasks");

    for (auto &t : drained) t();
    check(counter == 160, "Executed drained tasks (+20 +100 -> 160 total)");
}

// ─── Test 3: ThreadPoolConfig Singleton Customization ───────────────────────

void test_thread_pool_config_singleton() {
    std::cout << "\n[Test 3] ThreadPoolConfig singleton customization\n";

    auto &config = wavex::server::ThreadPoolConfig::instance();
    config.set_limits(2, 6);

    check(config.min_workers == 2, "Config min_workers set to 2");
    check(config.max_workers == 6, "Config max_workers set to 6");
    check(config.upper_thresholds.size() >= 5, "Upper threshold table sized appropriately for max 6 threads");
    check(config.lower_thresholds.size() >= 5, "Lower threshold table sized appropriately for max 6 threads");
}

// ─── Test 4: ThreadPool Execution & Dynamic Scaling ───────────────────────────

void test_thread_pool_execution_and_scaling() {
    std::cout << "\n[Test 4] ThreadPool execution, work-stealing & scaling\n";

    auto &config = wavex::server::ThreadPoolConfig::instance();
    config.set_limits(2, 4);
    config.upper_thresholds = {2, 5, 10};
    config.lower_thresholds = {1, 2, 4};

    wavex::server::ThreadPool pool(config);

    check(pool.worker_count() >= 2, "Initial worker count is at least min_workers (2)");

    std::atomic<int> executed_count{0};
    const int total_tasks = 20;

    for (int i = 0; i < total_tasks; ++i) {
        pool.dispatch([&executed_count] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            executed_count.fetch_add(1);
        });
    }

    // Give pool time to execute tasks and run scaling evaluation
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    check(executed_count.load() == total_tasks, "All 20 dispatched tasks executed cleanly");

    // Force scale evaluation after workload drops
    pool.evaluate_scaling();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    check(pool.worker_count() <= 4, "Worker count is bounded within max_workers limit (<= 4)");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== WaveX Server, HttpResponse & ThreadPool Unit Tests ===\n";

    test_http_response_serialization();
    test_work_stealing_queue();
    test_thread_pool_config_singleton();
    test_thread_pool_execution_and_scaling();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed.\n";
    return tests_passed == tests_run ? 0 : 1;
}
