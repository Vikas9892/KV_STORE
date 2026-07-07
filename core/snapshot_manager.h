#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class KVStore;
class WAL;

// Background thread that takes periodic snapshots of KVStore.
//
// Sequence on each snapshot:
//   1. get_all() under shared_lock  → consistent point-in-time copy
//   2. Snapshot::save()             → atomic tmp-rename write with CRC32
//   3. WAL::truncate()              → discard entries now captured in snapshot
//
// Destructor blocks until the background thread exits cleanly.
class SnapshotManager {
public:
    SnapshotManager(KVStore& store, WAL* wal,
                    std::string path,
                    std::chrono::seconds interval);
    ~SnapshotManager();

    SnapshotManager(const SnapshotManager&)            = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    // Take a snapshot immediately (blocks until complete).
    void trigger();

private:
    void loop();
    void do_snapshot();

    KVStore&                m_store;
    WAL*                    m_wal;
    std::string             m_path;
    std::chrono::seconds    m_interval;
    std::atomic<bool>       m_stop{false};
    std::mutex              m_cv_mutex;
    std::condition_variable m_cv;
    std::thread             m_thread;
};
