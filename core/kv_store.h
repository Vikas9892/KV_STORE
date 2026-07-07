#pragma once

#include "core/wal.h"

#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class KVStore {
public:
    explicit KVStore(std::string wal_path   = "kv.log",
                     std::string snap_path  = "kv.snap",
                     std::size_t initial_cap = 0);

    KVStore(const KVStore&)            = delete;
    KVStore& operator=(const KVStore&) = delete;

    void set(const std::string& key, std::string value);
    bool del(const std::string& key);
    void clear();

    std::optional<std::string> get(const std::string& key) const;
    bool        exists(const std::string& key) const;
    std::size_t size()   const;

    std::unordered_map<std::string, std::string> get_all() const;

    WAL& wal() { return m_wal; }

private:
    mutable std::shared_mutex                          m_mutex;
    std::unordered_map<std::string, std::string>       m_map;
    WAL                                                m_wal;
};
