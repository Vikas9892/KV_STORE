#include "core/snapshot_manager.h"
#include "core/kv_store.h"
#include "core/snapshot.h"
#include "core/wal.h"
#include "utils/logger.h"

#include <stdexcept>

SnapshotManager::SnapshotManager(KVStore& store, WAL* wal,
                                 std::string path,
                                 std::chrono::seconds interval)
    : m_store(store), m_wal(wal)
    , m_path(std::move(path)), m_interval(interval)
    , m_thread(&SnapshotManager::loop, this) {
    LOG_INFO("[snapshot_manager] Started — interval " +
             std::to_string(interval.count()) + "s → " + m_path);
}

SnapshotManager::~SnapshotManager() {
    m_stop = true;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void SnapshotManager::trigger() {
    do_snapshot();
}

void SnapshotManager::loop() {
    while (!m_stop) {
        std::unique_lock lock(m_cv_mutex);
        m_cv.wait_for(lock, m_interval, [this]{ return m_stop.load(); });
        if (m_stop) break;
        do_snapshot();
    }
}

void SnapshotManager::do_snapshot() {
    try {
        auto snap = m_store.get_all();      // consistent copy under shared_lock
        Snapshot::save(snap, m_path);
        if (m_wal) m_wal->truncate();       // WAL entries now captured in snapshot
    } catch (const std::exception& e) {
        LOG_ERROR("[snapshot_manager] Snapshot failed: " + std::string(e.what()));
    }
}
