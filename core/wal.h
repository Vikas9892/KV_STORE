#pragma once

#include <functional>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

class WAL {
public:
    using ReplayCb = std::function<void(std::string_view cmd,
                                        std::string_view key,
                                        std::string_view val)>;

    explicit WAL(std::string path);

    WAL(const WAL&)            = delete;
    WAL& operator=(const WAL&) = delete;

    void log_set(std::string_view key, std::string_view value);
    void log_del(std::string_view key);
    void log_clear();

    void replay(const ReplayCb& cb) const;

    // Discard all log entries — called after a successful snapshot so the
    // log only ever contains mutations newer than the snapshot.
    void truncate();

    const std::string& path() const { return m_path; }

private:
    std::string   m_path;
    std::ofstream m_file;
    std::mutex    m_mutex;
};
