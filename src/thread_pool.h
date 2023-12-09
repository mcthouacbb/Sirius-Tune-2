#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>

class ThreadPool
{
public:
    ThreadPool(uint32_t concurrency);
    ~ThreadPool();

    void wait();
    void addTask(const std::function<void()>& task);
private:
    void stop();
    void threadLoop();

    std::atomic_bool m_ShouldStop;
    std::condition_variable m_CV;
    std::vector<std::thread> m_Threads;
    std::mutex m_QueueLock;
    std::deque<std::function<void()>> m_Tasks;
};
