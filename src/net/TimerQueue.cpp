#include "net/TimerQueue.h"
#include "net/EventLoop.h"
#include "log/Log.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cassert>
#include <cstring> // for bzero
#include "net/Timer.h"

namespace
{

    // 创建一个 timerfd
    int createTimerfd()
    {
        // CLOCK_MONOTONIC: 从系统启动开始计时,不受系统时间修改影响
        // TFD_NONBLOCK | TFD_CLOEXEC: 设置为非阻塞,并在 exec 后关闭
        int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (timerfd < 0)
        {
            DLOG_FATAL << "Failed in timerfd_create"; // 使用你的日志库
            exit(1);
        }
        return timerfd;
    }

    // 计算从现在到指定时间点的时间差
    struct timespec howMuchTimeFromNow(Timestamp when)
    {
        int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
        if (microseconds < 100)
        { // 至少100微秒
            microseconds = 100;
        }
        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
        return ts;
    }

    // 读取 timerfd,以清除其可读事件,防止 epoll 重复触发
    void readTimerfd(int timerfd)
    {
        uint64_t howmany;
        ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
        DLOG_DEBUG << "TimerQueue::handleRead() " << howmany << " at " << Timestamp::now().toFormattedString();
        if (n != sizeof(howmany))
        {
            DLOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
        }
    }

    // 重置 timerfd 的到期时间
    void resetTimerfd(int timerfd, Timestamp expiration)
    {
        struct itimerspec newValue;
        struct itimerspec oldValue;
        bzero(&newValue, sizeof(newValue));
        bzero(&oldValue, sizeof(oldValue));
        newValue.it_value = howMuchTimeFromNow(expiration);
        int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
        if (ret)
        {
            DLOG_ERROR << "timerfd_settime() failed";
        }
    }

}

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback([this](Timestamp)
                                    { this->handleRead(); });
    // 将 timerfd 的读事件纳入 EventLoop 监听
    timerfdChannel_.enableReading();
}

// 析构函数：关闭 timerfd,自动释放所有定时器
TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove(); // 从 Poller 中移除
    ::close(timerfd_);
    // unique_ptr 会自动释放管理的 Timer 对象
}

// 添加定时器(线程安全,自动切换到 IO 线程)
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    auto timer = std::make_unique<Timer>(std::move(cb), when, interval);
    TimerId timerId(timer.get(), timer->sequence());

    if (loop_->isInLoopThread())
    {
        addTimerInLoop(std::move(timer));
    }
    else
    {
        Timer *raw_timer = timer.release();
        loop_->queueInLoop([this, raw_timer]()
                           {
            std::unique_ptr<Timer> t(raw_timer);
            this->addTimerInLoop(std::move(t)); });
    }
    return timerId;
}

// 取消定时器(线程安全,自动切换到 IO 线程)
void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop([this, timerId]
                     { this->cancelInLoop(timerId); });
}

// 实际添加定时器(只能在 IO 线程调用)
void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> timer)
{
    loop_->assertInLoopThread(); // 确保在正确的线程
    bool earliestChanged = insert(std::move(timer));

    if (earliestChanged)
    {
        // 如果插入的定时器是最早到期的,需要重置 timerfd
        resetTimerfd(timerfd_, timers_.begin()->first);
    }
}

// 实际取消定时器(只能在 IO 线程调用)
void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());

    ActiveTimer timerToCancel(timerId.timer_, timerId.sequence_);
    auto it = activeTimers_.find(timerToCancel);
    if (it != activeTimers_.end())
    {
        // 遍历timers_找到对应的Timer*,然后erase
        for (auto iter = timers_.begin(); iter != timers_.end(); ++iter)
        {
            if (iter->second.get() == timerToCancel.first && iter->second->sequence() == timerToCancel.second)
            {
                timers_.erase(iter);
                break;
            }
        }
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        // 如果正在处理到期事件,可能要取消的定时器就在到期列表中
        // 先将其加入 cancelingTimers_,在 reset 时会检查
        cancelingTimers_.insert(timerToCancel);
    }
    assert(timers_.size() == activeTimers_.size());
}

// timerfd 读事件回调,处理所有到期定时器
void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now = Timestamp::now();
    readTimerfd(timerfd_); // 清除事件,必须有

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // 依次执行到期定时器的回调
    for (const auto &it : expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    // 处理完后,重置周期性定时器
    reset(expired, now);
}

// 获取所有已到期的定时器,并移出 timers_
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    Entry sentry(now, nullptr);
    auto end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);

    // 只能move整个pair,不能拷贝
    for (auto it = timers_.begin(); it != end;)
    {
        expired.push_back({it->first, std::move(const_cast<std::unique_ptr<Timer> &>(it->second))});
        it = timers_.erase(it);
    }

    // 同时从 activeTimers_ 中移除
    for (const auto &entry : expired)
    {
        ActiveTimer timer(entry.second.get(), entry.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

// 重置周期性定时器,未被取消的重新插入
void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    for (auto &it : expired)
    {
        auto &timer = const_cast<Entry &>(it).second;
        if (timer && timer->repeat() && cancelingTimers_.find({timer.get(), timer->sequence()}) == cancelingTimers_.end())
        {
            timer->restart(now);
            insert(std::move(timer));
        }
    }

    Timestamp nextExpire;
    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->first;
    }
    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}

// 插入新定时器,返回是否需要重置 timerfd
bool TimerQueue::insert(std::unique_ptr<Timer> timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());

    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    auto it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    // 插入 timers_
    auto [iter, inserted] = timers_.insert({when, std::move(timer)});
    assert(inserted);
    // 插入 activeTimers_
    activeTimers_.insert({iter->second.get(), iter->second->sequence()});

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}