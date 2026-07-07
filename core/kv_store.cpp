#include "core/kv_store.h"

#include <shared_mutex>

void KVStore::set(const std::string& key, std::string value) {
    std::unique_lock lock(m_mutex);
    m_map.insert_or_assign(key, std::move(value));
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(m_mutex);
    return m_map.erase(key) != 0;
}

void KVStore::clear() {
    std::unique_lock lock(m_mutex);
    m_map.clear();
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
