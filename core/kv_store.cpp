#include "core/kv_store.h"
#include "core/snapshot.h"
#include "utils/logger.h"

KVStore::KVStore(std::string wal_path, std::string snap_path, std::size_t initial_cap)
    : m_wal(std::move(wal_path))
{
    if (initial_cap > 0) m_map.reserve(initial_cap);

    // 1. Load snapshot (fast binary restore)
    std::size_t n = Snapshot::load(snap_path, m_map);
    if (n > 0)
        LOG_INFO("[kv_store] snapshot loaded: " + std::to_string(n) + " entries");

    // 2. Replay WAL delta since last snapshot
    m_wal.replay([this](std::string_view cmd, std::string_view key, std::string_view val) {
        if (cmd == "SET")        m_map.insert_or_assign(std::string(key), std::string(val));
        else if (cmd == "DEL")   m_map.erase(std::string(key));
        else if (cmd == "CLEAR") m_map.clear();
    });
}

void KVStore::set(const std::string& key, std::string value) {
    std::unique_lock lock(m_mutex);
    m_map.insert_or_assign(key, std::move(value));
    m_wal.log_set(key, m_map.at(key));
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(m_mutex);
    bool erased = m_map.erase(key) != 0;
    if (erased) m_wal.log_del(key);
    return erased;
}

void KVStore::clear() {
    std::unique_lock lock(m_mutex);
    m_map.clear();
    m_wal.log_clear();
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    return it->second;
}

bool KVStore::exists(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    return m_map.count(key) != 0;
}

std::size_t KVStore::size() const {
    std::shared_lock lock(m_mutex);
    return m_map.size();
}

std::unordered_map<std::string, std::string> KVStore::get_all() const {
    std::shared_lock lock(m_mutex);
    return m_map;
}
