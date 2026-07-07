#include "server/replicator.h"
#include "utils/logger.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

// ── Construction / destruction ────────────────────────────────────────────────

Replicator::Replicator(std::string host, uint16_t port,
                       std::chrono::seconds reconnect_interval,
                       std::chrono::seconds heartbeat_interval)
    : m_host(std::move(host)), m_port(port)
    , m_reconnect_interval(reconnect_interval)
    , m_heartbeat_interval(heartbeat_interval)
    , m_thread(&Replicator::repl_loop, this) {
    LOG_INFO("[replicator] Started → " + m_host + ":" + std::to_string(m_port));
}

Replicator::~Replicator() {
    m_stop = true;
    m_queue_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
    if (m_fd >= 0) ::close(m_fd);
}

// ── Public API ────────────────────────────────────────────────────────────────

void Replicator::enqueue(std::string cmd) {
    {
        std::unique_lock lock(m_queue_mutex);
        m_queue.push(std::move(cmd));
    }
    m_queue_cv.notify_one();
}

void Replicator::forward_set(std::string_view key, std::string_view value) {
    enqueue("SET " + std::string(key) + " " + std::string(value));
}

void Replicator::forward_setex(std::string_view key, std::string_view value, int64_t ttl) {
    enqueue("SETEX " + std::string(key) + " " + std::to_string(ttl) + " " + std::string(value));
}

void Replicator::forward_del(std::string_view key) {
    enqueue("DELETE " + std::string(key));
}

void Replicator::forward_clear() {
    enqueue("CLEAR");
}

std::size_t Replicator::pending_count() const {
    std::unique_lock lock(m_queue_mutex);
    return m_queue.size();
}

// ── Network helpers ───────────────────────────────────────────────────────────

bool Replicator::try_connect() {
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }

    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    if (::inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr) != 1) {
        auto* he = ::gethostbyname(m_host.c_str());
        if (!he) { ::close(m_fd); m_fd = -1; return false; }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(in_addr));
    }

    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(m_fd); m_fd = -1; return false;
    }

    LOG_INFO("[replicator] Connected to replica " + m_host + ":" + std::to_string(m_port));
    m_connected = true;
    return true;
}

bool Replicator::send_cmd(const std::string& cmd) {
    std::string wire = cmd + "\r\n";
    std::size_t sent = 0;
    while (sent < wire.size()) {
        ssize_t n = ::send(m_fd, wire.data() + sent, wire.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) { m_connected = false; return false; }
        sent += static_cast<std::size_t>(n);
    }

    // Read and discard the response (we trust the replica will apply it)
    char buf[64]; char ch;
    while (::recv(m_fd, &ch, 1, 0) == 1) if (ch == '\n') break;
    return true;
}

// ── Replication loop ──────────────────────────────────────────────────────────

void Replicator::repl_loop() {
    auto last_heartbeat = std::chrono::steady_clock::now();

    while (!m_stop) {
        if (!m_connected) {
            if (!try_connect()) {
                LOG_WARN("[replicator] Cannot reach replica — retrying in "
                         + std::to_string(m_reconnect_interval.count()) + "s");
                std::unique_lock lock(m_queue_mutex);
                m_queue_cv.wait_for(lock, m_reconnect_interval,
                                    [this]{ return m_stop.load(); });
                continue;
            }
        }

        // Drain pending commands
        std::string cmd;
        {
            std::unique_lock lock(m_queue_mutex);
            auto deadline = std::chrono::steady_clock::now() + m_heartbeat_interval;
            m_queue_cv.wait_until(lock, deadline,
                [this]{ return m_stop.load() || !m_queue.empty(); });
            if (m_stop && m_queue.empty()) break;
            if (m_queue.empty()) {
                // Heartbeat — PING to detect dead connection
                if (!send_cmd("PING")) {
                    LOG_WARN("[replicator] Replica unreachable (heartbeat failed)");
                }
                continue;
            }
            cmd = std::move(m_queue.front());
            m_queue.pop();
        }

        if (!send_cmd(cmd)) {
            LOG_WARN("[replicator] Send failed — re-queuing and reconnecting");
            // Re-queue so no writes are lost
            std::unique_lock lock(m_queue_mutex);
            std::queue<std::string> tmp;
            tmp.push(std::move(cmd));
            while (!m_queue.empty()) { tmp.push(std::move(m_queue.front())); m_queue.pop(); }
            m_queue = std::move(tmp);
            m_connected = false;
        }
    }
}
