#include "core/kv_store.h"

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

// ── Basic ops ─────────────────────────────────────────────────────────────────

TEST(KVStore, SetAndGet) {
    KVStore s;
    s.set("name", "Vikas");
    auto v = s.get("name");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "Vikas");
}

TEST(KVStore, GetMissing) {
    KVStore s;
    EXPECT_FALSE(s.get("ghost").has_value());
}

TEST(KVStore, Overwrite) {
    KVStore s;
    s.set("k", "first");
    s.set("k", "second");
    EXPECT_EQ(*s.get("k"), "second");
}

TEST(KVStore, Delete) {
    KVStore s;
    s.set("x", "1");
    EXPECT_TRUE(s.del("x"));
    EXPECT_FALSE(s.get("x").has_value());
}

TEST(KVStore, DeleteMissing) {
    KVStore s;
    EXPECT_FALSE(s.del("nothing"));
}

TEST(KVStore, Exists) {
    KVStore s;
    s.set("a", "b");
    EXPECT_TRUE(s.exists("a"));
    EXPECT_FALSE(s.exists("z"));
}

TEST(KVStore, Size) {
    KVStore s;
    EXPECT_EQ(s.size(), 0u);
    s.set("k1", "v1");
    s.set("k2", "v2");
    EXPECT_EQ(s.size(), 2u);
    s.del("k1");
    EXPECT_EQ(s.size(), 1u);
}

TEST(KVStore, Clear) {
    KVStore s;
    s.set("a", "1"); s.set("b", "2");
    s.clear();
    EXPECT_EQ(s.size(), 0u);
    EXPECT_FALSE(s.get("a").has_value());
}

TEST(KVStore, ValueWithSpaces) {
    KVStore s;
    s.set("msg", "hello world foo");
    EXPECT_EQ(*s.get("msg"), "hello world foo");
}

// ── Thread safety ─────────────────────────────────────────────────────────────

TEST(KVStore, ConcurrentWrites) {
    KVStore              s;
    std::vector<std::thread> writers;
    constexpr int N = 8, OPS = 1000;

    for (int t = 0; t < N; ++t) {
        writers.emplace_back([&s, t]() {
            for (int i = 0; i < OPS; ++i)
                s.set("k" + std::to_string(t), std::to_string(i));
        });
    }
    for (auto& w : writers) w.join();
    EXPECT_EQ(s.size(), static_cast<std::size_t>(N));
}

TEST(KVStore, ConcurrentReadsAndWrites) {
    KVStore s;
    s.set("shared", "init");

    std::vector<std::thread> threads;
    constexpr int N = 4, OPS = 5000;

    for (int t = 0; t < N; ++t) {
        threads.emplace_back([&s, t]() {
            for (int i = 0; i < OPS; ++i) {
                if (i % 5 == 0) s.set("shared", std::to_string(t * OPS + i));
                else             (void)s.get("shared");
            }
        });
    }
    for (auto& t : threads) t.join();
    SUCCEED();  // no data race == pass (run under TSan to verify)
}
