#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include "base/noncopyable.h"

/**
 * @brief 业务线程池，用于处理耗时任务，避免阻塞IO线程
 *
 * 使用生产者-消费者模式，支持任务队列和多线程并发处理。
 */
class ThreadPool : noncopyable
{
public:
    using Task = std::function<void()>;

    /**
     * @brief 构造函数
     * @param threadNum 线程数量，默认为CPU核心数，最小为1
     */
    explicit ThreadPool(size_t threadNum = std::thread::hardware_concurrency());

    /**
     * @brief 析构函数，停止所有线程
     */
    ~ThreadPool();

    /**
     * @brief 提交任务到线程池
     * @param task 任务函数
     * @return true表示提交成功，false表示队列已满或线程池已停止
     */
    bool submit(Task task);

    /**
     * @brief 获取当前任务队列大小
     * @return 队列中待处理的任务数量
     */
    size_t queueSize() const;

    /**
     * @brief 设置任务队列最大容量（0表示无限制）
     * @param maxSize 最大容量
     */
    void setMaxQueueSize(size_t maxSize) { maxQueueSize_ = maxSize; }

private:
    /**
     * @brief 工作线程函数
     */
    void workerThread();

    std::vector<std::thread> workers_;      // 工作线程
    std::queue<Task> tasks_;                // 任务队列
    mutable std::mutex mutex_;              // 保护任务队列的互斥锁
    std::condition_variable condition_;     // 条件变量
    std::atomic<bool> stop_;                // 停止标志
    size_t maxQueueSize_;                   // 任务队列最大容量（0表示无限制）
};
