#include "threadpool/thread_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

TEST(ThreadPool, ExecutesAllTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    constexpr int N = 1000;

    for (int i = 0; i < N; ++i)
        pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });

    // Destructor joins all threads after draining queue
    // We need a brief wait before checking; let the pool drain.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(counter.load(), N);
}

TEST(ThreadPool, ThreadCount) {
    ThreadPool pool(8);
    EXPECT_EQ(pool.thread_count(), 8u);
}

TEST(ThreadPool, ConcurrentCounter) {
    ThreadPool          pool(4);
    std::atomic<int>    sum{0};
    constexpr int       N = 10'000;

    for (int i = 1; i <= N; ++i) {
        int val = i;
        pool.enqueue([&sum, val] {
            sum.fetch_add(val, std::memory_order_relaxed);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Sum 1..N = N*(N+1)/2
    EXPECT_EQ(sum.load(), N * (N + 1) / 2);
}

TEST(ThreadPool, CleanShutdown) {
    std::atomic<int> done{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 50; ++i)
            pool.enqueue([&done] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                done.fetch_add(1, std::memory_order_relaxed);
            });
    }  // destructor joins here — all tasks must complete
    EXPECT_EQ(done.load(), 50);
}
