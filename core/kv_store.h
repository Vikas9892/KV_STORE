#pragma once

#include "core/wal.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

using TimePoint = std::chrono::steady_clock::time_point;

class KVStore {
public:
    explicit KVStore(std::string wal_path   = "kv.log",
                     std::string snap_path  = "kv.snap",
                     std::size_t initial_cap = 0);
    ~KVStore();

    KVStore(const KVStore&)            = delete;
    KVStore& operator=(const KVStore&) = delete;

    // Write ops — exclusive lock + WAL append
    void set(const std::string& key, std::string value);
    bool del(const std::string& key);
    void clear();
    void setex(const std::string& key, std::string value, int64_t ttl_seconds);

    // Read ops — shared lock; lazy expiry on GET/EXISTS
    std::optional<std::string> get(const std::string& key);
    bool        exists(const std::string& key);
    std::size_t size() const;
    int64_t     ttl(const std::string& key) const;

    std::unordered_map<std::string, std::string> get_all() const;

    WAL& wal() { return m_wal; }

private:
    bool is_expired_locked(const std::string& key) const;
    void erase_if_expired(const std::string& key);
    void reaper_loop();

    mutable std::shared_mutex                          m_mutex;
    std::unordered_map<std::string, std::string>       m_map;
    std::unordered_map<std::string, TimePoint>         m_expiry;
    WAL                                                m_wal;

    std::atomic<bool> m_stop{false};
    std::thread       m_reaper;
};
