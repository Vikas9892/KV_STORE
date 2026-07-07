#include "network/socket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

static constexpr int RECV_CHUNK = 4096;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

Socket::~Socket() {
    if (m_fd >= 0) ::close(m_fd);
}

Socket::Socket(Socket&& o) noexcept : m_fd(o.m_fd), m_buf(std::move(o.m_buf)) {
    o.m_fd = -1;
}

Socket& Socket::operator=(Socket&& o) noexcept {
    if (this != &o) {
        if (m_fd >= 0) ::close(m_fd);
        m_fd   = o.m_fd;
        m_buf  = std::move(o.m_buf);
        o.m_fd = -1;
    }
    return *this;
}

// ── Factory ───────────────────────────────────────────────────────────────────

Socket Socket::listen_on(int port, int backlog) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::string("socket: ") + strerror(errno));

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("bind: ") + strerror(errno));
    }
    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("listen: ") + strerror(errno));
    }
    return Socket{fd};
}

// ── Operations ────────────────────────────────────────────────────────────────

Socket Socket::accept_client(std::string& peer_addr) const {
    sockaddr_in peer{};
    socklen_t   peerlen = sizeof(peer);

    int cfd = ::accept(m_fd, reinterpret_cast<sockaddr*>(&peer), &peerlen);
    if (cfd < 0)
        throw std::runtime_error(std::string("accept: ") + strerror(errno));

    char ip[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    peer_addr = std::string(ip) + ":" + std::to_string(ntohs(peer.sin_port));

    return Socket{cfd};
}

void Socket::send_all(std::string_view data) const {
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(m_fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += static_cast<std::size_t>(n);
    }
}

std::optional<std::string> Socket::recv_line() {
    char buf[RECV_CHUNK];

    while (true) {
        auto pos = m_buf.find('\n');
        if (pos != std::string::npos) {
            std::string line = m_buf.substr(0, pos);
            m_buf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
        ssize_t n = ::recv(m_fd, buf, sizeof(buf), 0);
        if (n <= 0) return std::nullopt;
        m_buf.append(buf, static_cast<std::size_t>(n));
    }
}
