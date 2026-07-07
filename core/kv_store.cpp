#include "core/kv_store.h"
#include "core/snapshot.h"
#include "utils/logger.h"

#include <chrono>

using namespace std::chrono;

KVStore::KVStore(std::string wal_path, std::string snap_path, std::size_t initial_cap)
    : m_wal(std::move(wal_path))
{
    if (initial_cap > 0) m_map.reserve(initial_cap);

    std::size_t n = Snapshot::load(snap_path, m_map);
    if (n > 0)
        LOG_INFO("[kv_store] snapshot loaded: " + std::to_string(n) + " entries");

    m_wal.replay([this](std::string_view cmd, std::string_view key, std::string_view val) {
        if (cmd == "SET")        m_map.insert_or_assign(std::string(key), std::string(val));
        else if (cmd == "DEL")   m_map.erase(std::string(key));
        else if (cmd == "CLEAR") m_map.clear();
    });

    m_reaper = std::thread([this]{ reaper_loop(); });
}

KVStore::~KVStore() {
    m_stop.store(true);
    if (m_reaper.joinable()) m_reaper.join();
}

bool KVStore::is_expired_locked(const std::string& key) const {
    auto it = m_expiry.find(key);
    if (it == m_expiry.end()) return false;
    return steady_clock::now() >= it->second;
}

void KVStore::erase_if_expired(const std::string& key) {
    if (is_expired_locked(key)) {
        m_map.erase(key);
        m_expiry.erase(key);
    }
}

void KVStore::reaper_loop() {
    while (!m_stop.load()) {
        std::this_thread::sleep_for(seconds(1));
        std::unique_lock lock(m_mutex);
        auto now = steady_clock::now();
        for (auto it = m_expiry.begin(); it != m_expiry.end(); ) {
            if (now >= it->second) {
                m_map.erase(it->first);
                it = m_expiry.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void KVStore::set(const std::string& key, std::string value) {
    std::unique_lock lock(m_mutex);
    m_expiry.erase(key);
    m_map.insert_or_assign(key, std::move(value));
    m_wal.log_set(key, m_map.at(key));
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(m_mutex);
    m_expiry.erase(key);
    bool erased = m_map.erase(key) != 0;
    if (erased) m_wal.log_del(key);
    return erased;
}

void KVStore::clear() {
    std::unique_lock lock(m_mutex);
    m_map.clear();
    m_map.rehash(0);
    m_expiry.clear();
    m_wal.log_clear();
}

void KVStore::setex(const std::string& key, std::string value, int64_t ttl_seconds) {
    std::unique_lock lock(m_mutex);
    m_map.insert_or_assign(key, std::move(value));
    m_expiry[key] = steady_clock::now() + seconds(ttl_seconds);
    m_wal.log_set(key, m_map.at(key));
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::unique_lock lock(m_mutex);
    erase_if_expired(key);
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    return it->second;
}

bool KVStore::exists(const std::string& key) {
    std::unique_lock lock(m_mutex);
    erase_if_expired(key);
    return m_map.count(key) != 0;
}

std::size_t KVStore::size() const {
    std::shared_lock lock(m_mutex);
    return m_map.size();
}

int64_t KVStore::ttl(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    if (!m_map.count(key)) return -2;
    auto it = m_expiry.find(key);
    if (it == m_expiry.end()) return -1;
    auto remaining = duration_cast<seconds>(it->second - steady_clock::now()).count();
    return remaining < 0 ? -2 : remaining;
}

std::unordered_map<std::string, std::string> KVStore::get_all() const {
    std::shared_lock lock(m_mutex);
    return m_map;
}
