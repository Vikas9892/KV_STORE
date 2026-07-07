#pragma once

#include "core/kv_store.h"
#include "network/socket.h"
#include "threadpool/thread_pool.h"

#include <cstddef>
#include <cstdint>

struct ServerConfig {
    uint16_t    port    = 7379;
    std::size_t workers = 0;    // 0 = hardware_concurrency
};

class TcpServer {
public:
    explicit TcpServer(const ServerConfig& cfg);

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void run();
    void stop();

private:
    Socket     m_listener;
    uint16_t   m_port;
    KVStore    m_store;
    ThreadPool m_pool;
};
