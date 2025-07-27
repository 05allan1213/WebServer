#include "EventLoopThread.h"

#include "EventLoop.h"

/**
 * @brief 构造函数
 * @param cb 线程初始化回调函数
 * @param name 线程名称
 * @param epollMode epoll模式，支持ET/LT
 * @details 创建EventLoopThread对象，但不立即启动线程
 */
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name, const std::string &epollMode)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb),
      epollMode_(epollMode)
{
}

/**
 * @brief 析构函数
 * @details 通知EventLoop退出循环，等待子线程结束
 */
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        // 通知子线程中的 EventLoop 退出循环
        loop_->quit();
        // 等待子线程执行完毕
        thread_.join();
    }
}

/**
 * @brief 启动EventLoop线程
 * @return 子线程中创建的EventLoop指针
 * @details 启动子线程，等待EventLoop创建完成，然后返回EventLoop指针
 */
EventLoop *EventLoopThread::startLoop()
{
    // 1. 启动底层的新线程
    thread_.start();

    EventLoop *loop = nullptr;
    {
        // 2. 等待子线程创建 EventLoop 对象并通知
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) // 子线程已创建 EventLoop 并赋值给 loop_
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    // 3. 返回子线程中创建的 EventLoop 对象的指针
    return loop;
}

/**
 * @brief 线程函数，在子线程中运行
 * @details 创建EventLoop对象，执行初始化回调，开始事件循环
 */
void EventLoopThread::threadFunc()
{
    // 1. 创建 EventLoop 对象,支持 ET/LT
    EventLoop loop(epollMode_); // 创建一个独立的eventloop,和上面的线程是一一对应的,one loop per thread

    // 2. 如果用户传入了初始化回调函数,则执行它
    if (callback_)
    {
        callback_(&loop);
    }

    {
        // 3. 获取互斥锁,准备修改共享变量 loop_ 并通知父线程
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    // 4. 开始事件循环
    // loop.loop() 会阻塞在这里,直到 loop.quit() 被调用
    loop.loop(); // EventLoop loop  => Poller.poll
    // --- loop.loop() 返回后,表示事件循环结束 ---

    // 5. 清理 loop_ 指针 (在退出前)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
    // 线程函数执行完毕,子线程即将退出
}