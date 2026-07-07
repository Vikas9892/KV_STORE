// Network benchmark using ConnectionPool — eliminates reconnect overhead.
// Usage: ./kv_netbench [host=127.0.0.1] [port=7379] [pool_size=16] [ops=50000]

#include "client/connection_pool.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<double, std::milli>;

static void worker(ConnectionPool& pool, std::size_t ops,
                   std::atomic<std::size_t>& done) {
    for (std::size_t i = 0; i < ops; ++i) {
        std::string key = "bench:" + std::to_string(i);
        pool.execute("SET " + key + " " + std::to_string(i));
        pool.execute("GET " + key);
        done.fetch_add(2, std::memory_order_relaxed);
    }
}

int main(int argc, char* argv[]) {
    const char* host    = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port    = (argc > 2) ? uint16_t(std::atoi(argv[2])) : 7379;
    std::size_t psize   = (argc > 3) ? std::size_t(std::atoi(argv[3])) : 16;
    std::size_t ops     = (argc > 4) ? std::size_t(std::atoi(argv[4])) : 50'000;
    std::size_t threads = psize;   // one producer thread per pool slot

    std::cout << "=== kv-store network benchmark (ConnectionPool) ===\n"
              << "server=" << host << ':' << port
              << "  pool=" << psize << "  threads=" << threads
              << "  ops/thread=" << ops << "\n\n";

    ConnectionPool pool(host, port, psize);

    std::atomic<std::size_t>  done{0};
    std::vector<std::thread>  workers;
    workers.reserve(threads);

    auto t0 = Clock::now();
    for (std::size_t i = 0; i < threads; ++i)
        workers.emplace_back(worker, std::ref(pool), ops, std::ref(done));
    for (auto& w : workers) w.join();
    double elapsed_ms = Ms(Clock::now() - t0).count();

    std::size_t total = done.load();
    double rps = static_cast<double>(total) / (elapsed_ms / 1000.0);
    double lat = (elapsed_ms * 1000.0) / static_cast<double>(total);

    std::cout << "Completed:   " << total << " ops in "
              << std::fixed << std::setprecision(1) << elapsed_ms << " ms\n"
              << "Throughput:  " << std::setprecision(0) << rps << " ops/sec\n"
              << "Avg latency: " << std::setprecision(3) << lat << " µs/op\n";
}
