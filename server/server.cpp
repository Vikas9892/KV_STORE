#include "server/server.h"
#include "network/socket.h"

#include <stdexcept>
#include <thread>

TcpServer::TcpServer(const ServerConfig& cfg)
    : m_listener(Socket::listen_on(cfg.port)), m_port(cfg.port) {}

void TcpServer::run() {
    while (true) {
        std::string peer;
        Socket sock = m_listener.accept_client(peer);
        // Phase 1: spawn a raw thread per connection (replaced by pool in Phase 7)
        std::thread([s = std::move(sock)]() mutable {
            char buf[256];
            while (true) {
                ssize_t n = ::recv(s.fd(), buf, sizeof(buf), 0);
                if (n <= 0) break;
                ::send(s.fd(), buf, static_cast<std::size_t>(n), MSG_NOSIGNAL);
            }
        }).detach();
    }
}

void TcpServer::stop() {}
