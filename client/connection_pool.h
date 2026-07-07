#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Client-side connection pool.
//
// Maintains N persistent TCP connections to the server. Callers borrow
// a connection via acquire(), use it, then return it via release(id).
// If all connections are in use, acquire() blocks until one is returned.
// Eliminates TCP handshake overhead in high-throughput benchmarks.
class ConnectionPool {
public:
    ConnectionPool(std::string host, uint16_t port, std::size_t size);
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    struct Lease {
        int          fd;
        std::size_t  id;
        ConnectionPool* pool;

        Lease(int f, std::size_t i, ConnectionPool* p) : fd(f), id(i), pool(p) {}
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& o) noexcept : fd(o.fd), id(o.id), pool(o.pool) { o.pool = nullptr; }
        ~Lease() { if (pool) pool->release(id); }
    };

    // Blocks until a connection is available, returns a RAII Lease.
    Lease acquire();

    // Execute a command and return the response line.
    // Acquires + releases a connection automatically.
    std::string execute(const std::string& cmd);

    std::size_t size() const { return m_conns.size(); }

private:
    struct Conn { int fd; bool busy; };

    void release(std::size_t id);

    std::string              m_host;
    uint16_t                 m_port;
    std::vector<Conn>        m_conns;
    std::mutex               m_mutex;
    std::condition_variable  m_cv;
};
