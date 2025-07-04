#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>


class ThreadPool
{
public:
    ThreadPool(uint32_t concurrency);
    ~ThreadPool();

    uint32_t concurrency() const
    {
        return m_Threads.size();
    }

    void wait();
    void addTask(const std::function<void()>& task);

private:
    void stop();
    void threadLoop();

    std::atomic_bool m_ShouldStop;
    std::condition_variable m_CV;
    std::vector<std::thread> m_Threads;
    std::mutex m_QueueLock;
    std::atomic_uint32_t m_RunningTasks;
    std::deque<std::function<void()>> m_Tasks;
};
