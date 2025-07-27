#include "EventLoop.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>

#include "Channel.h"
#include "Poller.h"
#include "log/Log.h"
#include "net/TimerQueue.h"
#include "base/Timestamp.h"
#include "net/TimerId.h"

// 保证一个线程内最多只能创建一个 EventLoop 对象
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的IO复用调用的超时时间
const int kPollTimeMs = 10000; // 默认10s

/**
 * @brief 创建事件文件描述符
 * @return 成功返回文件描述符，失败返回-1
 * @note 用于线程间通信，唤醒事件循环
 */
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        DLOG_FATAL << "eventfd error:" << errno;
    }
    return evtfd;
}

std::atomic<EventLoop *> EventLoop::mainLoop_{nullptr};
std::atomic_bool EventLoop::signalRegistered_{false};

/**
 * @brief 构造函数
 * @param epollMode epoll模式
 * @note 初始化事件循环，设置wakeup通道，注册信号处理器
 */
EventLoop::EventLoop(const std::string &epollMode)
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this, epollMode)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventFd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      epollMode_(epollMode)
//   currentActiveChannel_(nullptr)
{
    DLOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
    if (t_loopInThisThread)
    {
        DLOG_FATAL << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    // 设置wakeupfd的事件类型及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每个eventlop监听wakeupfd的EPOLLIN事件
    wakeupChannel_->enableReading();
    // 自动注册信号处理器,实现优雅退出
    registerSignalHandlerOnce(this);
}

/**
 * @brief 析构函数
 * @note 清理资源，关闭wakeup文件描述符
 */
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

/**
 * @brief 开启事件循环
 * @note 主循环，处理IO事件和定时器事件
 */
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    DLOG_INFO << "EventLoop " << this << " start looping";

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd   一种是client的fd,一种wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_) // 遍历 Poller 返回的所有发生了事件的 Channel
        {
            // 调用每个活跃Channel的处理方法
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环待处理的回调操作
        doPendingFunctors();
    }

    DLOG_INFO << "EventLoop " << this << " stop looping.";
    looping_ = false;
}

/**
 * @brief 退出事件循环
 * @note 支持两种调用方式：1.在loop自己的线程中调用quit 2.在非loop的线程中调用loop的quit
 */
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        // 在其他loop线程调用 quit
        wakeup(); // 唤醒阻塞在poll()上的目标loop,以退出循环
    }
    // 在loop自身线程调用quit,说明此时线程正在处理事件,处理完直接退出循环
}

/**
 * @brief 在当前loop中执行回调函数
 * @param cb 要执行的回调函数
 * @note 如果当前线程就是loop线程，直接执行；否则加入队列
 */
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}

/**
 * @brief 将回调函数放入队列，唤醒loop所在线程执行
 * @param cb 要执行的回调函数
 * @note 通过wakeup机制确保回调在正确的线程中执行
 */
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒逻辑:
    // 1. 如果调用 queueInLoop 的线程不是loop自己的线程 (通常情况)
    // 2. 如果是loop自己的线程在调用 queueInLoop,但它当前正在处理 pendingFunctors 队列
    //(意味着本次添加的 cb 不会在当前这轮 doPendingFunctors 中被执行,需要唤醒 loop让下一轮执行)
    // 则需要唤醒 EventLoop 线程
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

/**
 * @brief 处理wakeup文件描述符的读事件
 * @note 读取eventfd数据，清空缓冲区
 */
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        DLOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

/**
 * @brief 唤醒loop所在线程
 * @note 利用 eventfd 的特性,写操作会使其变为可读,从而被 Poller 检测到
 */
void EventLoop::wakeup()
{
    uint64_t one = 1;
    // 利用 eventfd 的特性,写操作会使其变为可读,从而被 Poller 检测到
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        DLOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

// EventLoop的方法 -> Poller的方法
/**
 * @brief 更新通道
 * @param channel 要更新的通道
 */
void EventLoop::updateChannel(Channel *channel) { poller_->updateChannel(channel); }

/**
 * @brief 移除通道
 * @param channel 要移除的通道
 */
void EventLoop::removeChannel(Channel *channel) { poller_->removeChannel(channel); }

/**
 * @brief 检查是否包含指定通道
 * @param channel 要检查的通道
 * @return 如果包含返回true，否则返回false
 */
bool EventLoop::hasChannel(Channel *channel) { return poller_->hasChannel(channel); }

/**
 * @brief 执行待处理的回调函数
 * @note 通过swap操作高效地处理回调队列，减少锁持有时间
 */
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;  // 1. 创建一个局部的临时 vector
    callingPendingFunctors_ = true; // 2. 设置标志位,表示正在处理回调

    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 3. swap 操作: 高效地将 pendingFunctors_ 的内容转移到局部 functors,同时将 pendingFunctors_
        // 清空,极大地缩短了锁的持有时间！
        functors.swap(pendingFunctors_);
    }

    // 4. 遍历并执行从队列中取出的所有回调
    for (const Functor &functor : functors)
    {
        functor(); // 执行回调
    }

    callingPendingFunctors_ = false; // 5. 清除标志位,表示处理完毕
}

/**
 * @brief 注册信号处理器（仅注册一次）
 * @param loop 主事件循环指针
 * @note 注册SIGINT和SIGTERM信号处理，支持优雅退出
 */
void EventLoop::registerSignalHandlerOnce(EventLoop *loop)
{
    if (!signalRegistered_.exchange(true))
    {
        mainLoop_ = loop;
        ::signal(SIGINT, signalHandler);
        ::signal(SIGTERM, signalHandler);
        DLOG_INFO << "[EventLoop] 已自动注册SIGINT/SIGTERM信号处理,支持优雅退出";
    }
}

/**
 * @brief 信号处理函数
 * @param signo 信号编号
 * @note 收到信号时优雅退出主事件循环
 */
void EventLoop::signalHandler(int signo)
{
    if (mainLoop_)
    {
        DLOG_INFO << "[EventLoop] 收到信号 " << signo << ",即将优雅退出...";
        mainLoop_.load()->quit();
    }
}

// 定时器相关接口实现
/**
 * @brief 在指定时间运行定时器
 * @param time 运行时间
 * @param cb 回调函数
 * @return 定时器ID
 */
TimerId EventLoop::runAt(Timestamp time, std::function<void()> cb)
{
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

/**
 * @brief 延迟指定时间后运行定时器
 * @param delay 延迟时间（秒）
 * @param cb 回调函数
 * @return 定时器ID
 */
TimerId EventLoop::runAfter(double delay, std::function<void()> cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

/**
 * @brief 周期性运行定时器
 * @param interval 间隔时间（秒）
 * @param cb 回调函数
 * @return 定时器ID
 */
TimerId EventLoop::runEvery(double interval, std::function<void()> cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

/**
 * @brief 取消定时器
 * @param timerId 定时器ID
 */
void EventLoop::cancel(TimerId timerId)
{
    return timerQueue_->cancel(timerId);
}

/**
 * @brief 断言当前线程是事件循环线程
 * @note 如果不是事件循环线程则终止程序
 */
void EventLoop::assertInLoopThread()
{
    if (!isInLoopThread())
    {
        abort();
    }
}