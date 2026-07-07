#include "server/server.h"
#include "server/client_session.h"
#include "utils/logger.h"

#include <chrono>
#include <thread>

TcpServer::TcpServer(const Config& cfg)
    : m_cfg(cfg)
    , m_store(cfg.wal_path, cfg.snapshot_path, cfg.kv_initial_capacity)
    , m_pool(cfg.workers == 0 ? std::thread::hardware_concurrency() : cfg.workers)
    , m_listener(Socket::listen_on(cfg.port))
{
    if (cfg.snapshot_interval_s > 0) {
        m_snap_mgr = std::make_unique<SnapshotManager>(
            m_store, &m_store.wal(),
            m_cfg.snapshot_path,
            std::chrono::seconds(cfg.snapshot_interval_s));
    }
    LOG_INFO("[server] port=" + std::to_string(cfg.port) +
             " workers=" + std::to_string(m_pool.thread_count()));
}

TcpServer::~TcpServer() {
    if (m_snap_mgr) m_snap_mgr->trigger();
}

void TcpServer::run() {
    while (true) {
        std::string peer;
        Socket sock = m_listener.accept_client(peer);
        m_pool.enqueue([s = std::move(sock), p = std::move(peer), this]() mutable {
            ClientSession(std::move(s), std::move(p), m_store).handle();
        });
    }
}

void TcpServer::stop() {}
