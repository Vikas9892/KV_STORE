#include "server/server.h"
#include "server/client_session.h"
#include "server/signal_handler.h"
#include "utils/logger.h"

#include <cerrno>
#include <chrono>
#include <sys/select.h>
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
    if (!cfg.replica_host.empty()) {
        m_replicator = std::make_unique<Replicator>(cfg.replica_host, cfg.replica_port);
        LOG_INFO("[server] replication → " + cfg.replica_host +
                 ":" + std::to_string(cfg.replica_port));
    }
    SignalHandler::install();
    LOG_INFO("[server] port=" + std::to_string(cfg.port) +
             " workers=" + std::to_string(m_pool.thread_count()));
}

TcpServer::~TcpServer() {
    if (m_snap_mgr) m_snap_mgr->trigger();
}

void TcpServer::run() {
    int listen_fd = m_listener.fd();
    int sig_fd    = SignalHandler::pipe_read_fd();
    int nfds      = std::max(listen_fd, sig_fd) + 1;

    while (true) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        FD_SET(sig_fd,    &rfds);

        if (::select(nfds, &rfds, nullptr, nullptr, nullptr) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (FD_ISSET(sig_fd, &rfds) || SignalHandler::triggered()) {
            LOG_INFO("[server] signal received — shutting down");
            break;
        }
        if (FD_ISSET(listen_fd, &rfds)) {
            std::string peer;
            Socket sock = m_listener.accept_client(peer);
            m_pool.enqueue([s = std::move(sock), p = std::move(peer), this]() mutable {
                ClientSession(std::move(s), std::move(p), m_store, m_replicator.get()).handle();
            });
        }
    }
}

void TcpServer::stop() {}
