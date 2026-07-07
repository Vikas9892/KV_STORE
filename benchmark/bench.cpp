// Direct KVStore benchmark — measures raw in-memory throughput without
// network overhead.  Run this to establish a ceiling before measuring
// the full server stack.
//
// Usage:  ./kv_bench [ops=1000000] [threads=4]

#include "core/kv_store.h"

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
using Ns    = std::chrono::duration<double, std::nano>;

static void print_result(const char* label, std::size_t ops, double elapsed_ms) {
    double rps     = static_cast<double>(ops) / (elapsed_ms / 1000.0);
    double lat_us  = (elapsed_ms * 1000.0) / static_cast<double>(ops);
    std::cout << std::left << std::setw(30) << label
              << std::right << std::setw(14) << std::fixed << std::setprecision(0)
              << rps << " ops/sec"
              << "  avg " << std::setprecision(3) << lat_us << " µs/op\n";
}

static double bench_set(KVStore& store, std::size_t ops) {
    auto t0 = Clock::now();
    for (std::size_t i = 0; i < ops; ++i)
        store.set("key:" + std::to_string(i), "value:" + std::to_string(i));
    return Ms(Clock::now() - t0).count();
}

static double bench_get(KVStore& store, std::size_t ops) {
    auto t0 = Clock::now();
    for (std::size_t i = 0; i < ops; ++i)
        (void)store.get("key:" + std::to_string(i % (ops / 2)));
    return Ms(Clock::now() - t0).count();
}

static double bench_concurrent(KVStore& store, std::size_t ops, std::size_t nthreads) {
    std::atomic<std::size_t> counter{0};
    std::vector<std::thread> workers;
    workers.reserve(nthreads);

    auto t0 = Clock::now();
    for (std::size_t t = 0; t < nthreads; ++t) {
        workers.emplace_back([&store, &counter, ops, nthreads]() {
            std::size_t per_thread = ops / nthreads;
            for (std::size_t i = 0; i < per_thread; ++i) {
                std::size_t id = counter.fetch_add(1, std::memory_order_relaxed);
                if (id % 4 == 0)
                    store.set("k:" + std::to_string(id), std::to_string(id));
                else
                    (void)store.get("k:" + std::to_string(id % (ops / 8)));
            }
        });
    }
    for (auto& w : workers) w.join();
    return Ms(Clock::now() - t0).count();
}

int main(int argc, char* argv[]) {
    std::size_t ops     = (argc > 1) ? static_cast<std::size_t>(std::atoll(argv[1])) : 1'000'000;
    std::size_t threads = (argc > 2) ? static_cast<std::size_t>(std::atoi(argv[2]))  : 4;

    std::cout << "=== kv-store benchmark (in-memory, no network) ===\n";
    std::cout << "ops=" << ops << "  threads=" << threads << "\n\n";

    KVStore store;

    double t_set = bench_set(store, ops);
    print_result("Sequential SET", ops, t_set);

    double t_get = bench_get(store, ops);
    print_result("Sequential GET (50% hit)", ops, t_get);

    double t_mix = bench_concurrent(store, ops, threads);
    print_result(("Concurrent 25%W/75%R (" + std::to_string(threads) + "T)").c_str(),
                 ops, t_mix);

    std::cout << "\nTotal time: " << std::fixed << std::setprecision(1)
              << (t_set + t_get + t_mix) << " ms\n";
}
