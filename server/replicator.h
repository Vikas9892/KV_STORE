#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Leader-side replication — forwards every write to a replica server.
//
// A background thread maintains a persistent TCP connection to the
// replica. When the leader executes SET / SETEX / DELETE / CLEAR it
// calls forward_*(), which enqueues the command. The replication thread
// drains the queue and sends it over the wire. If the replica is down,
// the thread reconnects every reconnect_interval_s seconds.
//
// On the replica side, run a plain kv_server — it receives the same
// commands as normal client traffic and applies them. No special mode
// needed on the replica.
class Replicator {
public:
    Replicator(std::string host, uint16_t port,
               std::chrono::seconds reconnect_interval = std::chrono::seconds(5),
               std::chrono::seconds heartbeat_interval = std::chrono::seconds(10));
    ~Replicator();

    Replicator(const Replicator&)            = delete;
    Replicator& operator=(const Replicator&) = delete;

    void forward_set(std::string_view key, std::string_view value);
    void forward_setex(std::string_view key, std::string_view value, int64_t ttl);
    void forward_del(std::string_view key);
    void forward_clear();

    bool is_connected() const { return m_connected.load(); }
    std::size_t pending_count() const;

private:
    void repl_loop();
    bool try_connect();
    bool send_cmd(const std::string& cmd);
    void enqueue(std::string cmd);

    std::string              m_host;
    uint16_t                 m_port;
    std::chrono::seconds     m_reconnect_interval;
    std::chrono::seconds     m_heartbeat_interval;

    int                      m_fd{-1};
    std::atomic<bool>        m_connected{false};
    std::atomic<bool>        m_stop{false};

    std::queue<std::string>  m_queue;
    mutable std::mutex       m_queue_mutex;
    std::condition_variable  m_queue_cv;

    std::thread              m_thread;
};
