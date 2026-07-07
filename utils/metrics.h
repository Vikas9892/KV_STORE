#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

// Lock-free metrics collection.
// All counters are std::atomic so any thread can update them safely
// without locking. Singleton via Metrics::get().
struct Metrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> cmd_set{0};
    std::atomic<uint64_t> cmd_get{0};
    std::atomic<uint64_t> cmd_delete{0};
    std::atomic<uint64_t> cmd_exists{0};
    std::atomic<uint64_t> cmd_ping{0};
    std::atomic<uint64_t> cmd_size{0};
    std::atomic<uint64_t> cmd_clear{0};
    std::atomic<uint64_t> cmd_errors{0};
    std::atomic<int64_t>  connected_clients{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_recv{0};

    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};

    static Metrics& get() {
        static Metrics instance;
        return instance;
    }

    uint64_t uptime_seconds() const {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<seconds>(steady_clock::now() - start_time).count());
    }

    std::string to_string() const {
        auto up = uptime_seconds();
        auto h  = up / 3600;
        auto m  = (up % 3600) / 60;
        auto s  = up % 60;

        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "uptime_seconds:%llu\n"
            "uptime_human:%02lluh%02llum%02llus\n"
            "connected_clients:%lld\n"
            "total_requests:%llu\n"
            "cmd_ping:%llu\n"
            "cmd_set:%llu\n"
            "cmd_get:%llu\n"
            "cmd_delete:%llu\n"
            "cmd_exists:%llu\n"
            "cmd_size:%llu\n"
            "cmd_clear:%llu\n"
            "cmd_errors:%llu\n"
            "bytes_sent:%llu\n"
            "bytes_recv:%llu",
            (unsigned long long)uptime_seconds(),
            (unsigned long long)h, (unsigned long long)m, (unsigned long long)s,
            (long long)connected_clients.load(),
            (unsigned long long)total_requests.load(),
            (unsigned long long)cmd_ping.load(),
            (unsigned long long)cmd_set.load(),
            (unsigned long long)cmd_get.load(),
            (unsigned long long)cmd_delete.load(),
            (unsigned long long)cmd_exists.load(),
            (unsigned long long)cmd_size.load(),
            (unsigned long long)cmd_clear.load(),
            (unsigned long long)cmd_errors.load(),
            (unsigned long long)bytes_sent.load(),
            (unsigned long long)bytes_recv.load());
        return buf;
    }

    Metrics(const Metrics&)            = delete;
    Metrics& operator=(const Metrics&) = delete;
private:
    Metrics() = default;
};

#define METRICS_INC(counter) Metrics::get().counter.fetch_add(1, std::memory_order_relaxed)
#define METRICS_ADD(counter, n) Metrics::get().counter.fetch_add(n, std::memory_order_relaxed)
