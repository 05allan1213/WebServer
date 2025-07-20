#pragma once

#include "base/Timestamp.h"
#include "net/TimerId.h"
#include "net/Channel.h" // 你的网络库中必须有 Channel 类
#include <vector>
#include <set>
#include <memory>
#include <functional>
#include <utility>
#include <atomic>
#include "net/Timer.h"

class EventLoop; // 前向声明

/**
 * @brief 定时器队列,每个 EventLoop 拥有一个
 *
 * 这是定时器模块的核心,负责管理所有的定时器。
 * 它通过一个 timerfd 将定时器事件整合进 EventLoop 的 I/O 复用中。
 *
 * 主要功能：
 * 1. 支持高效添加、取消定时器(线程安全)
 * 2. 精确到微秒级的定时任务调度
 * 3. 支持一次性和周期性定时器
 * 4. 采用 timerfd + epoll 实现高性能定时事件驱动
 * 5. 解决定时器 ABA 问题,保证取消/重置安全
 *
 * 设计说明：
 * - timers_ 用 std::set 按到期时间排序,管理所有定时器的所有权(unique_ptr)
 * - activeTimers_ 用于高效查找和取消定时器,避免 ABA 问题
 * - cancelingTimers_ 处理回调执行期间的取消请求
 * - 所有操作都保证线程安全,适合高并发场景
 */
class TimerQueue
{
public:
    using TimerCallback = std::function<void()>; // 定时器回调类型

    /**
     * @brief 构造函数
     * @param loop 所属的 EventLoop
     *
     * 初始化 timerfd、注册读事件回调,将定时器事件纳入 IO 复用
     */
    explicit TimerQueue(EventLoop *loop);
    /**
     * @brief 析构函数
     *
     * 关闭 timerfd,自动释放所有定时器资源
     */
    ~TimerQueue();

    /**
     * @brief 添加一个定时器(线程安全)
     * @param cb 回调函数
     * @param when 到期时间
     * @param interval 重复间隔(秒),为0表示一次性定时器
     * @return 用于取消该定时器的 TimerId
     *
     * 可在任意线程调用,内部自动切换到 IO 线程执行
     */
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    /**
     * @brief 取消一个定时器(线程安全)
     * @param timerId 要取消的定时器的 ID
     *
     * 可在任意线程调用,内部自动切换到 IO 线程执行
     */
    void cancel(TimerId timerId);

private:
    // Entry for storing in the set
    // 使用 unique_ptr 来管理 Timer 对象的生命周期
    using Entry = std::pair<Timestamp, std::unique_ptr<Timer>>; // 定时器集合元素类型
    // Timers are sorted by expiration time
    using TimerList = std::set<Entry>; // 按到期时间排序的定时器集合

    // For cancel()
    // A set of active timers' pointers for quick lookup during cancellation.
    using ActiveTimer = std::pair<Timer *, int64_t>; // 活跃定时器辅助查找结构(指针+序列号)
    using ActiveTimerSet = std::set<ActiveTimer>;    // 活跃定时器集合

    /**
     * @brief 实际添加定时器的实现(只能在 IO 线程调用)
     * @param timer 待添加的定时器对象
     *
     * 插入 timers_ 集合,并根据需要重置 timerfd
     */
    void addTimerInLoop(std::unique_ptr<Timer> timer);
    /**
     * @brief 实际取消定时器的实现(只能在 IO 线程调用)
     * @param timerId 要取消的定时器 ID
     *
     * 支持在回调执行期间的安全取消
     */
    void cancelInLoop(TimerId timerId);

    /**
     * @brief timerfd 的读事件回调
     *
     * 处理所有到期定时器,执行回调并重置周期性定时器
     */
    void handleRead();

    /**
     * @brief 获取所有已到期的定时器
     * @param now 当前时间
     * @return 到期定时器的 Entry 列表
     *
     * 会将到期的定时器从 timers_ 中移除,并返回所有权
     */
    std::vector<Entry> getExpired(Timestamp now);

    /**
     * @brief 重置已到期的定时器(如果是周期性任务)
     * @param expired 已到期的定时器列表
     * @param now 当前时间
     *
     * 对于周期性定时器,重新计算下次到期时间并插入 timers_
     */
    void reset(const std::vector<Entry> &expired, Timestamp now);

    /**
     * @brief 向 timers_ 集合中插入一个新定时器
     * @param timer 待插入的定时器对象
     * @return 是否需要重置 timerfd(新定时器比当前所有定时器都早到期)
     *
     * 插入 timers_ 和 activeTimers_,保证定时器唯一性和高效查找
     */
    bool insert(std::unique_ptr<Timer> timer);

    EventLoop *loop_;        // 所属的 EventLoop
    const int timerfd_;      // 用于产生定时事件的 timerfd
    Channel timerfdChannel_; // 用于观察 timerfd_ 上的可读事件
    TimerList timers_;       // 按到期时间排序的定时器列表(拥有所有权)

    ActiveTimerSet activeTimers_;            // 活跃定时器集合,用于高效取消
    std::atomic<bool> callingExpiredTimers_; // 是否正在处理到期事件(防止回调期间取消冲突)
    ActiveTimerSet cancelingTimers_;         // 回调执行期间要取消的定时器集合
};