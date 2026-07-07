#pragma once

#include "core/kv_store.h"
#include "network/protocol.h"
#include "threadpool/thread_pool.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

// Non-blocking I/O server using Linux epoll (edge-triggered).
//
// A single event-loop thread calls epoll_wait() on O_NONBLOCK sockets.
// When a client fd is readable, the full available bytes are read into a
// per-connection buffer; complete lines are dispatched to the ThreadPool
// for command execution. Responses are written back on the event-loop
// thread. This handles tens of thousands of simultaneous connections with
// far less memory than one-thread-per-connection.
//
// Architecture:
//   accept loop (this thread)
//     └─ epoll_wait() → readable fds
//          ├─ new connection → EPOLLET | EPOLLIN
//          └─ data ready    → read all, dispatch to pool, write response
class EpollServer {
public:
    explicit EpollServer(uint16_t port,
                         KVStore& store,
                         std::size_t num_workers = 0);
    ~EpollServer();

    EpollServer(const EpollServer&)            = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void run();   // blocks until stop() is called

private:
    struct ConnState {
        std::string read_buf;
        std::string write_buf;
    };

    void add_fd(int fd);
    void remove_fd(int fd);
    void handle_readable(int fd);
    std::string dispatch(const std::string& line);

    int                                  m_epfd;
    int                                  m_listen_fd;
    uint16_t                             m_port;
    KVStore&                             m_store;
    ThreadPool                           m_pool;
    std::unordered_map<int, ConnState>   m_conns;
};
