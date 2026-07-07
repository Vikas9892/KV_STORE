#include "threadpool/thread_pool.h"

ThreadPool::ThreadPool(std::size_t num_threads) {
    m_workers.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i)
        m_workers.emplace_back(&ThreadPool::worker_loop, this);
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();  // wake every worker so they see m_stop

    for (auto& t : m_workers)
        t.join();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock lock(m_mutex);
        m_queue.push(std::move(task));
    }
    m_cv.notify_one();  // wake exactly one idle worker
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock(m_mutex);
            // Block until there's work to do OR shutdown is requested.
            // Spurious wakeups are safe because the lambda re-checks the condition.
            m_cv.wait(lock, [this] {
                return m_stop || !m_queue.empty();
            });

            if (m_stop && m_queue.empty()) return;  // drain before exit

            task = std::move(m_queue.front());
            m_queue.pop();
        }

        task();  // execute outside the lock so other workers can pick up tasks
    }
}
