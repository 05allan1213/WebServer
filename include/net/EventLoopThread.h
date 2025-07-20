#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "Thread.h"
#include "base/noncopyable.h"

class EventLoop;

/**
 * @brief EventLoop线程类，封装了一个运行EventLoop的线程
 *
 * EventLoopThread实现了"one loop per thread"的设计模式，每个线程运行一个EventLoop。
 * 主要功能：
 * - 在新线程中创建和运行EventLoop
 * - 提供线程安全的EventLoop访问接口
 * - 支持线程初始化回调函数
 * - 管理线程的生命周期
 *
 * 设计特点：
 * - 使用条件变量确保EventLoop在返回前已完全初始化
 * - 提供线程安全的startLoop接口
 * - 支持自定义线程名称和初始化回调
 *
 * 这个类通常被EventLoopThreadPool使用，用于创建多个IO线程。
 */
class EventLoopThread : noncopyable
{
public:
    /**
     * @brief 线程初始化回调函数类型
     * @param loop 新创建的EventLoop指针
     *
     * 当EventLoop在新线程中创建完成后调用，用于线程特定的初始化工作
     */
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    /**
     * @brief 构造函数
     * @param cb 线程初始化回调函数，默认为空
     * @param name 线程名称，默认为空字符串
     * @param epollMode epoll触发模式（"ET"/"LT"），默认LT
     *
     * 创建EventLoopThread对象，但不立即启动线程
     */
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string(),
                    const std::string &epollMode = "LT");

    /**
     * @brief 析构函数
     *
     * 停止线程，清理资源
     */
    ~EventLoopThread();

    /**
     * @brief 启动线程并在新线程中创建运行EventLoop
     * @return 返回新创建的EventLoop指针
     *
     * 这是线程安全的接口，会等待EventLoop在新线程中完全初始化后才返回。
     * 如果线程已经启动，直接返回现有的EventLoop指针。
     */
    EventLoop *startLoop();

private:
    /**
     * @brief 线程函数
     *
     * 在新线程中执行，创建EventLoop并运行事件循环
     */
    void threadFunc();

    EventLoop *loop_;              // 指向在子线程中创建的EventLoop对象
    bool exiting_;                 // 标记是否正在退出，防止重复启动
    Thread thread_;                // 线程对象，管理线程的生命周期
    std::mutex mutex_;             // 互斥锁，保护loop_的访问，确保线程安全
    std::condition_variable cond_; // 条件变量，用于startLoop等待loop_初始化完成
    ThreadInitCallback callback_;  // EventLoop创建后的初始化回调函数
    std::string epollMode_;        // epoll触发模式，"ET"=边缘触发，"LT"=水平触发
};