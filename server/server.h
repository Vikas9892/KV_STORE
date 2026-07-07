#pragma once

#include "core/kv_store.h"
#include "network/socket.h"

#include <cstdint>

struct ServerConfig {
    uint16_t port = 7379;
};

class TcpServer {
public:
    explicit TcpServer(const ServerConfig& cfg);

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void run();
    void stop();

private:
    Socket   m_listener;
    uint16_t m_port;
    KVStore  m_store;
};
