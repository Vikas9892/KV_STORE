#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Fixed-size thread pool — producer/consumer with condition_variable.
//
// Workers block on m_cv when the queue is empty. Enqueue() pushes a
// task and wakes one sleeping worker. On destruction, a poison-pill
// flag sets m_stop = true and broadcasts to all workers so they drain
// remaining tasks and exit cleanly.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void enqueue(std::function<void()> task);

    std::size_t thread_count() const { return m_workers.size(); }

private:
    void worker_loop();

    std::vector<std::thread>          m_workers;
    std::queue<std::function<void()>> m_queue;
    std::mutex                        m_mutex;
    std::condition_variable           m_cv;
    bool                              m_stop = false;
};
