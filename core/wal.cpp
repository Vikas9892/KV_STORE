#include "core/wal.h"
#include "utils/logger.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

WAL::WAL(std::string path) : m_path(std::move(path)) {
    // Open in append mode; creates the file if it doesn't exist
    m_file.open(m_path, std::ios::app);
    if (!m_file)
        throw std::runtime_error("WAL: cannot open log file: " + m_path);
}

void WAL::log_set(std::string_view key, std::string_view value) {
    std::unique_lock lock(m_mutex);
    m_file << "SET " << key << ' ' << value << '\n';
    m_file.flush();
}

void WAL::log_del(std::string_view key) {
    std::unique_lock lock(m_mutex);
    m_file << "DEL " << key << '\n';
    m_file.flush();
}

void WAL::log_clear() {
    std::unique_lock lock(m_mutex);
    m_file << "CLEAR\n";
    m_file.flush();
}

void WAL::replay(const ReplayCb& cb) const {
    std::ifstream in(m_path);
    if (!in) return;  // no log yet — first run

    std::string line;
    std::size_t count = 0;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string        cmd;
        ss >> cmd;

        if (cmd == "SET") {
            std::string key, value;
            ss >> key;
            std::getline(ss >> std::ws, value);
            cb(cmd, key, value);
        } else if (cmd == "DEL") {
            std::string key;
            ss >> key;
            cb(cmd, key, {});
        } else if (cmd == "CLEAR") {
            cb(cmd, {}, {});
        }
        ++count;
    }
    LOG_INFO("[WAL] replayed " + std::to_string(count) + " entries from " + m_path);
}

void WAL::truncate() {
    std::unique_lock lock(m_mutex);
    m_file.close();
    m_file.open(m_path, std::ios::trunc | std::ios::app);
    if (!m_file)
        throw std::runtime_error("WAL: truncate failed on " + m_path);
    LOG_INFO("[WAL] truncated after snapshot — " + m_path);
}
