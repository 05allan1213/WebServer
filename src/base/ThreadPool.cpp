#include "base/ThreadPool.h"
#include "log/Log.h"
#include <algorithm>

ThreadPool::ThreadPool(size_t threadNum)
    : stop_(false), maxQueueSize_(0)
{
    // 确保至少有1个工作线程
    threadNum = std::max(size_t(1), threadNum);
    DLOG_INFO << "ThreadPool 初始化，线程数: " << threadNum;
    for (size_t i = 0; i < threadNum; ++i)
    {
        workers_.emplace_back([this]
                              { workerThread(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for (std::thread &worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    DLOG_INFO << "ThreadPool 已停止";
}

bool ThreadPool::submit(Task task)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_)
        {
            DLOG_WARN << "ThreadPool 已停止，无法提交任务";
            return false;
        }
        // 检查队列是否已满
        if (maxQueueSize_ > 0 && tasks_.size() >= maxQueueSize_)
        {
            DLOG_WARN << "ThreadPool 任务队列已满，当前大小: " << tasks_.size();
            return false;
        }
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
    return true;
}

size_t ThreadPool::queueSize() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::workerThread()
{
    while (true)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]
                            { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty())
            {
                return;
            }

            if (!tasks_.empty())
            {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task)
        {
            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                DLOG_ERROR << "ThreadPool 任务执行异常: " << e.what();
            }
            catch (...)
            {
                DLOG_ERROR << "ThreadPool 任务执行未知异常";
            }
        }
    }
}
