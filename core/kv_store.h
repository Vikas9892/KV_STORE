#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class KVStore {
public:
    void set(const std::string& key, std::string value);
    bool del(const std::string& key);
    void clear();

    std::optional<std::string> get(const std::string& key) const;
    bool        exists(const std::string& key) const;
    std::size_t size()   const;

private:
    mutable std::mutex                           m_mutex;
    std::unordered_map<std::string, std::string> m_map;
};
