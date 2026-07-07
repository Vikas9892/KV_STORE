#include "client/connection_pool.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

static int connect_to(const char* host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        auto* he = ::gethostbyname(host);
        if (!he) { ::close(fd); throw std::runtime_error("resolve failed: " + std::string(host)); }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(in_addr));
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd); throw std::runtime_error("connect() failed");
    }
    return fd;
}

static std::string recv_line(int fd) {
    std::string line; char ch;
    while (::recv(fd, &ch, 1, 0) == 1) {
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

// ── ConnectionPool ────────────────────────────────────────────────────────────

ConnectionPool::ConnectionPool(std::string host, uint16_t port, std::size_t size)
    : m_host(std::move(host)), m_port(port) {
    m_conns.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        int fd = connect_to(m_host.c_str(), m_port);
        m_conns.push_back({fd, false});
    }
}

ConnectionPool::~ConnectionPool() {
    for (auto& c : m_conns) if (c.fd >= 0) ::close(c.fd);
}

ConnectionPool::Lease ConnectionPool::acquire() {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] {
        for (auto& c : m_conns) if (!c.busy) return true;
        return false;
    });
    for (std::size_t i = 0; i < m_conns.size(); ++i) {
        if (!m_conns[i].busy) {
            m_conns[i].busy = true;
            return Lease{m_conns[i].fd, i, this};
        }
    }
    throw std::runtime_error("acquire: no connection available"); // unreachable
}

void ConnectionPool::release(std::size_t id) {
    {
        std::unique_lock lock(m_mutex);
        m_conns[id].busy = false;
    }
    m_cv.notify_one();
}

std::string ConnectionPool::execute(const std::string& cmd) {
    auto lease = acquire();
    std::string req = cmd + "\r\n";
    ::send(lease.fd, req.data(), req.size(), MSG_NOSIGNAL);
    return recv_line(lease.fd);
}
