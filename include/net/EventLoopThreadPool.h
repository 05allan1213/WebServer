#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    // 线程初始化回调函数类型
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    // 设置线程池中的线程数量
    void setThreadNum(int numThreads) { threadNum_ = numThreads; }
    // 启动线程池
    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    // 获取下一个 EventLoop
    EventLoop *getNextLoop();
    // 获取线程池中所有的 EventLoop 指针
    std::vector<EventLoop *> getAllLoops();
    // 检查线程池是否已启动
    bool started() const { return started_; }
    // 获取线程池名称
    const std::string &name() const { return name_; }

private:
    EventLoop *baseLoop_; // 用户创建的主EventLoop
    std::string name_;    // 线程池名称
    bool started_;        // 线程池是否已启动
    int threadNum_;       // 线程池中线程的数量
    int next_;            // 用于 getNextLoop() 轮询的下一个索引
    std::vector<std::unique_ptr<EventLoopThread>>
        threads_;                    // 存储 EventLoopThread 对象的智能指针数组
    std::vector<EventLoop *> loops_; // 存储线程池中所有 EventLoop 的指针
};