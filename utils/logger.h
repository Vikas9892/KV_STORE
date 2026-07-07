#pragma once

#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <string_view>

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

// Thread-safe, level-filtered logger. Single global instance via get().
// Output goes to stderr; flushed on every message so interleaved threads
// produce complete lines.
class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    void set_level(LogLevel l) { m_level = l; }
    LogLevel level() const     { return m_level; }

    void debug(std::string_view msg) { log(LogLevel::DEBUG, msg); }
    void info (std::string_view msg) { log(LogLevel::INFO,  msg); }
    void warn (std::string_view msg) { log(LogLevel::WARN,  msg); }
    void error(std::string_view msg) { log(LogLevel::ERROR, msg); }

private:
    Logger() = default;

    void log(LogLevel level, std::string_view msg) {
        if (level < m_level) return;

        static const char* labels[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
        const char* label = labels[static_cast<int>(level)];

        std::time_t t = std::time(nullptr);
        char ts[20];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

        std::lock_guard lock(m_mutex);
        std::fprintf(stderr, "%s [%s] %.*s\n",
                     ts, label,
                     static_cast<int>(msg.size()), msg.data());
        std::fflush(stderr);
    }

    std::mutex m_level_mutex;   // guards m_level on concurrent set_level calls
    std::mutex m_mutex;
    LogLevel   m_level = LogLevel::INFO;
};

// Convenience macros build the string lazily — filtered levels pay no
// allocation cost if the string would never be printed.
#define LOG_DEBUG(msg) Logger::get().debug(msg)
#define LOG_INFO(msg)  Logger::get().info(msg)
#define LOG_WARN(msg)  Logger::get().warn(msg)
#define LOG_ERROR(msg) Logger::get().error(msg)
