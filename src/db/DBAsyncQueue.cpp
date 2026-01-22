#include "db/DBAsyncQueue.h"
#include "log/Log.h"

DBAsyncQueue::DBAsyncQueue(size_t workerNum)
    : stop_(false), pool_(DBConnectionPool::getInstance())
{
    DLOG_INFO << "DBAsyncQueue 初始化，工作线程数: " << workerNum;
    for (size_t i = 0; i < workerNum; ++i)
    {
        workers_.emplace_back([this]
                              { workerThread(); });
    }
}

DBAsyncQueue::~DBAsyncQueue()
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
    DLOG_INFO << "DBAsyncQueue 已停止";
}

void DBAsyncQueue::submit(DBTask task, ResultCallback callback)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_)
        {
            DLOG_WARN << "DBAsyncQueue 已停止，无法提交任务";
            return;
        }
        tasks_.push({std::move(task), std::move(callback)});
    }
    condition_.notify_one();
}

size_t DBAsyncQueue::queueSize() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}

void DBAsyncQueue::workerThread()
{
    while (true)
    {
        TaskItem item;
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
                item = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (item.task)
        {
            Connection *conn = nullptr;
            try
            {
                // 从连接池获取连接（可能阻塞，但不在锁内）
                conn = pool_->getConnection();
                if (conn)
                {
                    // 执行数据库操作
                    item.task(conn);
                }
                else
                {
                    DLOG_ERROR << "DBAsyncQueue 无法获取数据库连接";
                }
            }
            catch (const std::exception &e)
            {
                DLOG_ERROR << "DBAsyncQueue 任务执行异常: " << e.what();
            }
            catch (...)
            {
                DLOG_ERROR << "DBAsyncQueue 任务执行未知异常";
            }

            // 归还连接
            if (conn)
            {
                pool_->releaseConnection(conn);
            }

            // 执行回调（如果有）
            if (item.callback)
            {
                try
                {
                    item.callback();
                }
                catch (const std::exception &e)
                {
                    DLOG_ERROR << "DBAsyncQueue 回调执行异常: " << e.what();
                }
                catch (...)
                {
                    DLOG_ERROR << "DBAsyncQueue 回调执行未知异常";
                }
            }
        }
    }
}
