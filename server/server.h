#pragma once

#include "config/config.h"
#include "core/kv_store.h"
#include "core/snapshot_manager.h"
#include "network/socket.h"
#include "server/replicator.h"
#include "threadpool/thread_pool.h"

#include <memory>

class TcpServer {
public:
    explicit TcpServer(const Config& cfg);
    ~TcpServer();

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void run();
    void stop();

private:
    Config     m_cfg;
    KVStore    m_store;
    ThreadPool m_pool;
    std::unique_ptr<SnapshotManager> m_snap_mgr;
    std::unique_ptr<Replicator>      m_replicator;
    Socket     m_listener;
};
