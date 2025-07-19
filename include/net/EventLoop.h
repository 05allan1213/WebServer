#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unistd.h>
#include <vector>

#include "CurrentThread.h"
#include "Timestamp.h"
#include "base/noncopyable.h"

class Channel;
class Poller;
class TimerQueue;
// 事件循环类，主要包含两大模块 Channel 和 Poller(epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>; // 用于存储需要在 EventLoop 线程中执行的回调函数

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    // 返回当前loop执行的poller返回时间点
    Timestamp pollReturnTime() const { return pollReturnTime_; }
    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    // 唤醒loop所在线程
    void wakeup();

    // EventLoop的方法 -> Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断当前loop对象是否在自己的线程中
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    // 处理wakeup()
    void handleRead();
    // 执行回调
    void doPendingFunctors();

private:
    using ChannelList = std::vector<Channel *>; // 用于存储 Poller 返回的活跃 Channel

    std::atomic_bool looping_; // 标志loop是否在运行
    std::atomic_bool quit_;    // 标志退出loop循环

    const pid_t threadId_; // 记录当前loop所在线程的id

    Timestamp pollReturnTime_; // poller返回发生事件的channels的返回时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;                           // 用于跨线程通知 wakeupLoop 的fd
    std::unique_ptr<Channel> wakeupChannel_; // 封装wakeupFd_的channel对象

    ChannelList activeChannels_; // 存储poller返回的当前有事件发生的channel列表
    // Channel* currentActiveChannel_;  // 指向当前正在处理事件的channel

    std::atomic_bool callingPendingFunctors_; // 标志当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                        // 互斥锁,用来保护上面vector容器的线程安全操作
};