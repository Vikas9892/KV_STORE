#include "server/server.h"
#include "server/client_session.h"
#include "utils/logger.h"

#include <thread>

TcpServer::TcpServer(const Config& cfg)
    : m_cfg(cfg)
    , m_listener(Socket::listen_on(cfg.port))
    , m_pool(cfg.workers == 0 ? std::thread::hardware_concurrency() : cfg.workers)
{
    LOG_INFO("[server] port=" + std::to_string(cfg.port) +
             " workers=" + std::to_string(m_pool.thread_count()));
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
