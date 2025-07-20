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

/**
 * @brief 事件循环类，网络库的核心组件
 *
 * EventLoop是Reactor模式的核心，负责事件分发和回调处理。每个线程最多只能有一个EventLoop实例，
 * 采用"one loop per thread"的设计模式。主要包含两大模块：
 * - Channel：封装了文件描述符和事件回调
 * - Poller：IO多路复用的抽象层（如epoll）
 *
 * EventLoop通过以下机制实现线程安全：
 * - 使用eventfd进行跨线程唤醒
 * - 通过pendingFunctors_队列处理跨线程回调
 * - 使用原子变量和互斥锁保护共享数据
 */
class EventLoop : noncopyable
{
public:
    /** @brief 用于存储需要在EventLoop线程中执行的回调函数类型 */
    using Functor = std::function<void()>;

    /**
     * @brief 构造函数
     * @param epollMode epoll触发模式（"ET"/"LT"），默认LT
     *
     * 创建EventLoop实例，初始化poller、wakeupfd等组件，确保线程唯一性
     */
    EventLoop(const std::string &epollMode = "LT");

    /**
     * @brief 析构函数
     *
     * 清理资源，关闭wakeupfd，重置线程局部变量
     */
    ~EventLoop();

    /**
     * @brief 开启事件循环
     *
     * 这是EventLoop的核心方法，会阻塞当前线程直到调用quit()。
     * 在循环中不断调用poller监听事件，处理活跃的Channel，执行待处理的回调函数。
     */
    void loop();

    /**
     * @brief 退出事件循环
     *
     * 设置退出标志，如果在其他线程调用会唤醒EventLoop线程
     */
    void quit();

    /**
     * @brief 获取poller返回的时间戳
     * @return 最近一次poller返回的时间点
     */
    Timestamp pollReturnTime() const { return pollReturnTime_; }

    /**
     * @brief 在当前EventLoop线程中执行回调函数
     * @param cb 要执行的回调函数
     *
     * 如果当前线程就是EventLoop所在线程，直接执行；否则将回调加入队列
     */
    void runInLoop(Functor cb);

    /**
     * @brief 将回调函数加入队列，唤醒EventLoop线程执行
     * @param cb 要执行的回调函数
     *
     * 这是跨线程调用的主要接口，通过wakeup机制确保回调在正确的线程中执行
     */
    void queueInLoop(Functor cb);

    /**
     * @brief 唤醒EventLoop所在线程
     *
     * 通过向wakeupfd写入数据来唤醒阻塞在poll()上的EventLoop线程
     */
    void wakeup();

    /**
     * @brief 更新Channel在Poller中的监听状态
     * @param channel 要更新的Channel指针
     */
    void updateChannel(Channel *channel);

    /**
     * @brief 从Poller中移除Channel
     * @param channel 要移除的Channel指针
     */
    void removeChannel(Channel *channel);

    /**
     * @brief 检查Channel是否在Poller中注册
     * @param channel 要检查的Channel指针
     * @return 如果Channel已注册返回true，否则返回false
     */
    bool hasChannel(Channel *channel);

    /**
     * @brief 判断当前EventLoop对象是否在自己的线程中
     * @return 如果在EventLoop所在线程中返回true，否则返回false
     */
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    /**
     * @brief 处理wakeupfd的读事件
     *
     * 当其他线程调用wakeup()时，EventLoop线程会收到通知，读取wakeupfd中的数据
     */
    void handleRead();

    /**
     * @brief 执行待处理的回调函数
     *
     * 通过swap操作高效地取出所有待处理的回调函数并执行，减少锁的持有时间
     */
    void doPendingFunctors();

    // 自动优雅退出相关
    static void registerSignalHandlerOnce(EventLoop *loop);
    static void signalHandler(int signo);
    static std::atomic<EventLoop *> mainLoop_;
    static std::atomic_bool signalRegistered_;

private:
    /** @brief 用于存储Poller返回的活跃Channel列表 */
    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_; // 标志EventLoop是否正在运行
    std::atomic_bool quit_;    // 标志是否请求退出EventLoop循环

    const pid_t threadId_; // 记录当前EventLoop所在线程的ID，用于线程安全检查

    Timestamp pollReturnTime_;       // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; // IO多路复用器，负责监听文件描述符事件

    int wakeupFd_;                           // 用于跨线程通知的eventfd文件描述符
    std::unique_ptr<Channel> wakeupChannel_; // 封装wakeupFd_的Channel对象

    ChannelList activeChannels_; // 存储poller返回的当前有事件发生的channel列表
    // Channel* currentActiveChannel_;  // 指向当前正在处理事件的channel

    std::atomic_bool callingPendingFunctors_; // 标志当前EventLoop是否正在执行回调操作
    std::vector<Functor> pendingFunctors_;    // 存储EventLoop需要执行的所有回调操作
    std::mutex mutex_;                        // 互斥锁，保护pendingFunctors_的线程安全操作

    std::string epollMode_; // epoll触发模式，"ET"=边缘触发，"LT"=水平触发
};