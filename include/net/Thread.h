#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include "base/noncopyable.h"

class Thread : noncopyable
{
public:
    // 线程函数的类型别名
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    // 获取线程状态
    bool started() const { return started_; }
    // 获取线程 ID
    pid_t tid() const { return tid_; }
    // 获取线程名称
    const std::string &name() const { return name_; }
    // 获取已创建线程的总数
    static int numCreated() { return numCreated_; }

private:
    // 设置默认线程名
    void setDefaultName();

private:
    bool started_;                        // 线程是否已启动
    bool joined_;                         // 线程是否已 join
    std::shared_ptr<std::thread> thread_; // 线程对象
    pid_t tid_;                           // 线程 ID
    ThreadFunc func_;                     // 线程函数
    std::string name_;                    // 线程名称
    static std::atomic_int numCreated_;   // 记录所有 Thread 对象创建的线程数量
};