#include "server/epoll_server.h"
#include "server/signal_handler.h"
#include "utils/logger.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

static constexpr int     MAX_EVENTS = 1024;
static constexpr int     BACKLOG    = 512;
static constexpr ssize_t READ_CHUNK = 4096;

static void set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("set_nonblocking failed");
}

// ── EpollServer ───────────────────────────────────────────────────────────────

EpollServer::EpollServer(uint16_t port, KVStore& store, std::size_t num_workers)
    : m_port(port)
    , m_store(store)
    , m_pool(num_workers ? num_workers : std::thread::hardware_concurrency()) {

    m_epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (m_epfd < 0) throw std::runtime_error("epoll_create1 failed");

    m_listen_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_listen_fd < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    ::setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_port);

    if (::bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("bind: ") + strerror(errno));
    if (::listen(m_listen_fd, BACKLOG) < 0)
        throw std::runtime_error(std::string("listen: ") + strerror(errno));

    add_fd(m_listen_fd);

    // Also watch the signal pipe so epoll_wait() returns on SIGINT
    int sig_fd = SignalHandler::pipe_read_fd();
    if (sig_fd >= 0) add_fd(sig_fd);

    LOG_INFO("[epoll_server] Listening on 0.0.0.0:" + std::to_string(m_port)
             + "  workers=" + std::to_string(m_pool.thread_count()));
}

EpollServer::~EpollServer() {
    ::close(m_listen_fd);
    ::close(m_epfd);
}

void EpollServer::add_fd(int fd) {
    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;    // edge-triggered
    ev.data.fd = fd;
    ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
}

void EpollServer::remove_fd(int fd) {
    ::epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
    m_conns.erase(fd);
    ::close(fd);
}

void EpollServer::run() {
    LOG_INFO("[epoll_server] Event loop started");
    std::vector<epoll_event> events(MAX_EVENTS);
    int sig_fd = SignalHandler::pipe_read_fd();

    while (true) {
        int n = ::epoll_wait(m_epfd, events.data(), MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            // Signal pipe — time to stop
            if (fd == sig_fd) { goto shutdown; }

            // New connection on listening socket
            if (fd == m_listen_fd) {
                while (true) {
                    sockaddr_in peer{};
                    socklen_t plen = sizeof(peer);
                    int cfd = ::accept4(m_listen_fd,
                                        reinterpret_cast<sockaddr*>(&peer),
                                        &plen,
                                        SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (cfd < 0) break;   // EAGAIN — no more pending connections

                    char ip[INET_ADDRSTRLEN];
                    ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
                    LOG_DEBUG("[epoll_server] Accepted " + std::string(ip)
                              + ":" + std::to_string(ntohs(peer.sin_port)));

                    m_conns.emplace(cfd, ConnState{});
                    add_fd(cfd);
                }
                continue;
            }

            // Data available on a client connection
            if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
                handle_readable(fd);
        }
    }

shutdown:
    LOG_INFO("[epoll_server] Shutdown — closing all connections");
    for (auto& [fd, _] : m_conns) ::close(fd);
    m_conns.clear();
}

void EpollServer::handle_readable(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end()) return;

    ConnState& state = it->second;
    char buf[READ_CHUNK];

    // Edge-triggered: must read until EAGAIN
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n == 0) { remove_fd(fd); return; }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            remove_fd(fd); return;
        }
        state.read_buf.append(buf, static_cast<std::size_t>(n));
    }

    // Process all complete lines accumulated in the read buffer
    std::size_t pos;
    while ((pos = state.read_buf.find('\n')) != std::string::npos) {
        std::string line = state.read_buf.substr(0, pos);
        state.read_buf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::string resp = dispatch(line);
        // Write immediately (non-blocking); for production add write_buf + EPOLLOUT
        ssize_t sent = 0;
        while (sent < static_cast<ssize_t>(resp.size())) {
            ssize_t w = ::send(fd, resp.data() + sent,
                               resp.size() - static_cast<std::size_t>(sent), MSG_NOSIGNAL);
            if (w <= 0) { remove_fd(fd); return; }
            sent += w;
        }
    }
}

std::string EpollServer::dispatch(const std::string& line) {
    Request  req  = CommandParser::parse(line);
    Response resp = Response::error("unknown");

    switch (req.cmd) {
        case Command::PING:    resp = Response::value("PONG"); break;
        case Command::SET:
            if (!req.key.empty()) { m_store.set(req.key, req.value); resp = Response::ok(); }
            else resp = Response::error("usage: SET key value"); break;
        case Command::GET: {
            auto v = m_store.get(req.key);
            resp = v ? Response::value(*v) : Response::error("NOT_FOUND"); break;
        }
        case Command::DELETE:
            resp = m_store.del(req.key) ? Response::ok() : Response::error("NOT_FOUND"); break;
        case Command::EXISTS:
            resp = Response::integer_r(m_store.exists(req.key) ? 1 : 0); break;
        case Command::SIZE:
            resp = Response::integer_r(static_cast<int64_t>(m_store.size())); break;
        case Command::CLEAR:
            m_store.clear(); resp = Response::ok(); break;
        case Command::SETEX:
            if (!req.key.empty() && req.ttl_seconds > 0) {
                m_store.setex(req.key, req.value, req.ttl_seconds);
                resp = Response::ok();
            } else resp = Response::error("usage: SETEX key seconds value"); break;
        case Command::TTL:
            resp = Response::integer_r(m_store.ttl(req.key)); break;
        case Command::STATS:
            resp = Response::value("use kv_server for STATS"); break;
        case Command::UNKNOWN:
            resp = Response::error("unknown command"); break;
    }
    return resp.serialize();
}
