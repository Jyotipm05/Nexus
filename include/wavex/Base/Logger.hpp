/**
 * @file Logger.hpp
 * @brief Trantor-style leveled logger for WaveX.
 *
 * Provides 6 severity levels (TRACE -> FATAL), a global singleton,
 * thread-safe output, and WX_LOG_* convenience macros. Higher levels
 * are for production, lower levels for development and debugging.
 */

#pragma once

#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <mutex>
#include <optional>
#include <format>
#include <chrono>
#include <thread>
#include <source_location>
#include <cstdlib>
#include <filesystem>

namespace wavex::base {
    /**
     * @enum LogLevel
     * @brief Severity levels for the WaveX logging system.
     */
    enum class LogLevel : uint8_t {
        TRACE = 0, ///< Fine-grained debug: function entry/exit, buffer dumps
        DEBUG = 1, ///< Development diagnostics: route matching, codec details
        INFO = 2, ///< Lifecycle events: startup, listening, worker counts
        WARN = 3, ///< Recoverable issues: timeouts, retries, deprecation
        ERROR = 4, ///< Request failures, socket errors, parse failures
        FATAL = 5 ///< Unrecoverable — logs then calls std::abort()
    };

    /**
     * @brief Returns a short string tag for a log level.
     */
    constexpr std::string_view log_level_tag(LogLevel lvl) {
        switch (lvl) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO ";
            case LogLevel::WARN: return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "?????";
        }
    }

    /**
     * @class Logger
     * @brief Thread-safe, singleton logger with leveled output.
     *
     * Usage:
     *   Logger::instance().set_level(LogLevel::DEBUG);
     *   WX_LOG_INFO("Server listening on port {}", 8080);
     */
    class Logger {
    public:
        /// Global singleton access
        static Logger &instance() {
            static Logger inst;
            return inst;
        }

        /// Set the minimum log level — anything below is discarded
        void set_level(const LogLevel level = LogLevel::INFO) { min_level_ = level; }

        /// Get the current minimum log level
        [[nodiscard]] LogLevel level() const { return min_level_; }

        /// Direct output to a different stream (default: std::cerr)
        void set_output(std::ostream &os) {
            std::lock_guard lock(mutex_);
            file_sink_.reset();
            sink_ = &os;
        }

        /// Direct output to a file
        void set_output(const std::filesystem::path &path = "./logs/wavex.log") {
            std::lock_guard lock(mutex_);
            file_sink_.emplace(path, std::ios::app);
            if (file_sink_->is_open()) {
                sink_ = &*file_sink_;
            }
        }

        /**
         * @brief Core logging function. Formats the message, prepends
         *        timestamp + level + thread ID, and writes to sink.
         *
         * @tparam Args  Format argument types
         * @param lvl    Severity level
         * @param fmt    std::format-compatible format string
         * @param args   Format arguments
         */
        template<typename... Args>
        void log(const LogLevel lvl,
                 std::format_string<Args...> fmt,
                 Args &&... args) {
            if (lvl < min_level_) return;
            const std::string user_msg = std::format(fmt, std::forward<Args>(args)...);
            write(lvl, user_msg);
        }

        /**
         * @brief Overload for plain string messages (no format args).
         */
        void log(const LogLevel lvl, const std::string_view msg) {
            if (lvl < min_level_) return;
            write(lvl, msg);
        }

        Logger(const Logger &) = delete;

        Logger &operator=(const Logger &) = delete;

    private:
        Logger() = default;

        void write(const LogLevel lvl, std::string_view msg) {
            const auto now = std::chrono::system_clock::now();
            auto time = std::chrono::floor<std::chrono::milliseconds>(now);
            auto tid = std::this_thread::get_id();

            // Format: [2026-06-13 15:30:05.123] [INFO ] [tid:1234] message
            const std::string line = std::format("[{}] [{}] [tid:{}] {}\n",
                                                 time, log_level_tag(lvl), tid, msg);

            std::lock_guard lock(mutex_);
            (*sink_) << line;
            sink_->flush();
        }

        LogLevel min_level_ = LogLevel::INFO;
        std::ostream *sink_ = &std::cerr;
        std::optional<std::ofstream> file_sink_;
        std::mutex mutex_;
    };
}


/**  Convenience macros — compile-time level check via if constexpr isn't
 *  possible with runtime level, so we use a cheap runtime branch.
 *  The format call is only evaluated if the level passes.
 */

#define WX_LOG_TRACE(...) \
    do { if (::wavex::base::Logger::instance().level() <= ::wavex::base::LogLevel::TRACE) \
        ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::TRACE, __VA_ARGS__); } while(0)

#define WX_LOG_DEBUG(...) \
    do { if (::wavex::base::Logger::instance().level() <= ::wavex::base::LogLevel::DEBUG) \
        ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::DEBUG, __VA_ARGS__); } while(0)

#define WX_LOG_INFO(...) \
    do { if (::wavex::base::Logger::instance().level() <= ::wavex::base::LogLevel::INFO) \
        ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::INFO, __VA_ARGS__); } while(0)

#define WX_LOG_WARN(...) \
    do { if (::wavex::base::Logger::instance().level() <= ::wavex::base::LogLevel::WARN) \
        ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::WARN, __VA_ARGS__); } while(0)

#define WX_LOG_ERROR(...) \
    do { if (::wavex::base::Logger::instance().level() <= ::wavex::base::LogLevel::ERROR) \
        ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::ERROR, __VA_ARGS__); } while(0)

#define WX_LOG_FATAL(...) \
    do { ::wavex::base::Logger::instance().log(::wavex::base::LogLevel::FATAL, __VA_ARGS__); \
         std::abort(); } while(0)
