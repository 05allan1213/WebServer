#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include "db/DBConnectionPool.h"
#include "base/noncopyable.h"

/**
 * @brief 数据库异步任务队列
 *
 * 将数据库操作放入队列，由专门的工作线程处理，避免在锁内长时间阻塞。
 * 支持异步执行和结果回调。
 */
class DBAsyncQueue : noncopyable
{
public:
    using DBTask = std::function<void(Connection *)>;
    using ResultCallback = std::function<void()>;

    /**
     * @brief 构造函数
     * @param workerNum 工作线程数量
     */
    explicit DBAsyncQueue(size_t workerNum = 2);

    /**
     * @brief 析构函数
     */
    ~DBAsyncQueue();

    /**
     * @brief 提交数据库任务
     * @param task 数据库操作任务，接收Connection*参数
     * @param callback 可选的结果回调，在任务完成后在IO线程中执行
     */
    void submit(DBTask task, ResultCallback callback = nullptr);

    /**
     * @brief 获取当前任务队列大小
     * @return 队列中待处理的任务数量
     */
    size_t queueSize() const;

private:
    struct TaskItem
    {
        DBTask task;
        ResultCallback callback;
    };

    /**
     * @brief 工作线程函数
     */
    void workerThread();

    std::vector<std::thread> workers_;      // 工作线程
    std::queue<TaskItem> tasks_;            // 任务队列
    mutable std::mutex mutex_;              // 保护任务队列的互斥锁
    std::condition_variable condition_;     // 条件变量
    std::atomic<bool> stop_;                // 停止标志
    DBConnectionPool *pool_;                // 数据库连接池
};
