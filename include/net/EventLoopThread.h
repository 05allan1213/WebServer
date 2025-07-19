#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "Thread.h"
#include "base/noncopyable.h"

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    // 线程初始化回调函数类型
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    // 启动线程,并在新线程中创建和运行 EventLoop
    EventLoop *startLoop();

private:
    // 线程函数
    void threadFunc();

    EventLoop *loop_;              // 指向在子线程中创建的 EventLoop 对象
    bool exiting_;                 // 标记是否正在退出
    Thread thread_;                // 线程对象
    std::mutex mutex_;             // 互斥锁，保护loop_的访问
    std::condition_variable cond_; // 条件变量，用于 startLoop 等待 loop_ 初始化完成
    ThreadInitCallback callback_;  // EventLoop 创建后的初始化回调
};