#include "server/server.h"
#include "server/client_session.h"
#include "network/socket.h"

#include <stdexcept>
#include <thread>

TcpServer::TcpServer(const ServerConfig& cfg)
    : m_listener(Socket::listen_on(cfg.port))
    , m_port(cfg.port)
    , m_pool(cfg.workers == 0 ? std::thread::hardware_concurrency() : cfg.workers) {}

void TcpServer::run() {
    while (true) {
        std::string peer;
        Socket sock = m_listener.accept_client(peer);
        // Pool bounds memory vs. spawning unbounded threads per connection
        m_pool.enqueue([s = std::move(sock), p = std::move(peer), this]() mutable {
            ClientSession(std::move(s), std::move(p), m_store).handle();
        });
    }
}

void TcpServer::stop() {}
