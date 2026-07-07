#include "server/server.h"
#include "server/client_session.h"
#include "network/socket.h"

#include <stdexcept>

TcpServer::TcpServer(const ServerConfig& cfg)
    : m_listener(Socket::listen_on(cfg.port)), m_port(cfg.port) {}

void TcpServer::run() {
    while (true) {
        std::string peer;
        Socket sock = m_listener.accept_client(peer);
        // OOP: delegate each connection to a ClientSession (synchronous for now)
        ClientSession(std::move(sock), std::move(peer), m_store).handle();
    }
}

void TcpServer::stop() {}
