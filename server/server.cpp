#include "server/server.h"
#include "server/client_session.h"
#include "network/socket.h"

#include <stdexcept>
#include <thread>

TcpServer::TcpServer(const ServerConfig& cfg)
    : m_listener(Socket::listen_on(cfg.port)), m_port(cfg.port) {}

void TcpServer::run() {
    while (true) {
        std::string peer;
        Socket sock = m_listener.accept_client(peer);
        // Spawn one std::thread per connection; detach so the server loop
        // can immediately accept the next client.
        std::thread([s = std::move(sock), p = std::move(peer), this]() mutable {
            ClientSession(std::move(s), std::move(p), m_store).handle();
        }).detach();
    }
}

void TcpServer::stop() {}
