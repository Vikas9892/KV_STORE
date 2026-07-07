#pragma once

#include "config/config.h"
#include "core/kv_store.h"
#include "network/socket.h"
#include "threadpool/thread_pool.h"

class TcpServer {
public:
    explicit TcpServer(const Config& cfg);

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void run();
    void stop();

private:
    Config     m_cfg;
    Socket     m_listener;
    KVStore    m_store;
    ThreadPool m_pool;
};
