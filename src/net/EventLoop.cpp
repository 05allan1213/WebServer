#include "EventLoop.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "Channel.h"
#include "Poller.h"
#include "log/Log.h"

// 保证一个线程内最多只能创建一个 EventLoop 对象
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的IO复用调用的超时时间
const int kPollTimeMs = 10000; // 默认10s

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        DLOG_FATAL << "eventfd error:" << errno;
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventFd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
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
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    DLOG_INFO << "EventLoop " << this << " start looping";

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd   一种是client的fd，一种wakeupfd
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

// 退出事件循环   1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        // 在其他loop线程调用 quit
        wakeup(); // 唤醒阻塞在poll()上的目标loop，以退出循环
    }
    // 在loop自身线程调用quit,说明此时线程正在处理事件，处理完直接退出循环
}

// 在当前loop中执行cb
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

// 把cb放入队列，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒逻辑:
    // 1. 如果调用 queueInLoop 的线程不是loop自己的线程 (通常情况)
    // 2. 如果是loop自己的线程在调用 queueInLoop，但它当前正在处理 pendingFunctors 队列
    //(意味着本次添加的 cb 不会在当前这轮 doPendingFunctors 中被执行，需要唤醒 loop让下一轮执行)
    // 则需要唤醒 EventLoop 线程
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        DLOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

// 唤醒loop所在线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    // 利用 eventfd 的特性，写操作会使其变为可读，从而被 Poller 检测到
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        DLOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

// EventLoop的方法 -> Poller的方法
void EventLoop::updateChannel(Channel *channel) { poller_->updateChannel(channel); }

void EventLoop::removeChannel(Channel *channel) { poller_->removeChannel(channel); }

bool EventLoop::hasChannel(Channel *channel) { return poller_->hasChannel(channel); }

// 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;  // 1. 创建一个局部的临时 vector
    callingPendingFunctors_ = true; // 2. 设置标志位，表示正在处理回调

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

    callingPendingFunctors_ = false; // 5. 清除标志位，表示处理完毕
}