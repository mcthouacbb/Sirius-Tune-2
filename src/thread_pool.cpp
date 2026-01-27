#include "thread_pool.h"

ThreadPool::ThreadPool(u32 concurrency)
    : m_ShouldStop(false), m_RunningTasks(0)
{
    for (u32 i = 0; i < concurrency; i++)
    {
        m_Threads.push_back(std::thread(
            [this]
            {
                threadLoop();
            }));
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::wait()
{
    std::unique_lock<std::mutex> uniqueLock(m_QueueLock);
    m_CV.wait(uniqueLock,
        [this]()
        {
            return m_Tasks.size() == 0 && m_RunningTasks == 0;
        });
}

void ThreadPool::addTask(const std::function<void()>& task)
{
    std::lock_guard guard(m_QueueLock);
    m_Tasks.push_back(task);
    m_CV.notify_one();
}

void ThreadPool::stop()
{
    m_ShouldStop = true;
    m_CV.notify_all();
    for (auto& thread : m_Threads)
        thread.join();
}

void ThreadPool::threadLoop()
{
    while (true)
    {
        std::unique_lock<std::mutex> uniqueLock(m_QueueLock);
        m_CV.notify_all();
        m_CV.wait(uniqueLock,
            [this]()
            {
                return m_ShouldStop || m_Tasks.size() > 0;
            });

        if (m_ShouldStop)
            return;

        std::function<void()> task = m_Tasks.front();
        m_Tasks.pop_front();
        m_RunningTasks++;
        uniqueLock.unlock();

        task();

        m_RunningTasks--;
    }
}
