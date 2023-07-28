#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace LeoVK
{
    class Thread
    {
    public:
        Thread();
        virtual ~Thread();
        void AddJob(std::function<void()> function);
        void Wait();

    private:
        void queueLoop();

    private:

        bool                    mbDestroying = false;
        std::thread             mWorker;
        std::queue<std::function<void()>> mJobQueue;
        std::mutex              mQueueMutex;
        std::condition_variable mCondition;
    };

    class ThreadPool
    {
    public:
        void SetThreadCount(uint32_t count);
        void Wait();

    public:
        std::vector<std::unique_ptr<Thread>> mThreads;
    };

    Thread::Thread()
    {
        mWorker = std::thread(&Thread::queueLoop, this);
    }

    Thread::~Thread()
    {
        if (mWorker.joinable())
        {
            Wait();
            mQueueMutex.lock();
            mbDestroying = true;
            mCondition.notify_one();
            mQueueMutex.unlock();
            mWorker.join();
        }
    }

    void Thread::AddJob(std::function<void()> function)
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mJobQueue.push(std::move(function));
        mCondition.notify_one();
    }

    void Thread::Wait()
    {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mCondition.wait(lock, [this]() { return mJobQueue.empty(); });
    }

    void ThreadPool::SetThreadCount(uint32_t count)
    {
        mThreads.clear();
        for (auto i = 0; i < count; i++) mThreads.push_back(std::make_unique<Thread>());
    }

    void ThreadPool::Wait()
    {
        for (auto & t : mThreads) t->Wait();
    }

    void Thread::queueLoop()
    {
        while (true)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mCondition.wait(lock, [this] { return !mJobQueue.empty() || mbDestroying; });
                if (mbDestroying) break;
                job = mJobQueue.front();
            }
            job();
            {
                std::lock_guard<std::mutex> lock(mQueueMutex);
                mJobQueue.pop();
                mCondition.notify_one();
            }
        }
    }
}