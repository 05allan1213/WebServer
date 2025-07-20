#pragma once

#include "base/Timestamp.h"
#include <functional>
#include <atomic>
#include <utility>

/**
 * @brief 定时器任务的内部表示
 *
 * 封装了一个定时任务的所有信息,包括回调函数、到期时间、是否重复等。
 * 这个类的对象由 TimerQueue 完全拥有。
 *
 * 设计说明：
 * - 支持一次性和周期性定时器
 * - 每个定时器有唯一序列号,便于高效查找和取消
 * - 线程安全的全局序列号生成
 */
class Timer
{
public:
    using TimerCallback = std::function<void()>; // 定时器回调类型

    /**
     * @brief 构造函数
     * @param cb 回调函数
     * @param when 到期时间
     * @param interval 重复间隔(秒),为0表示一次性定时器
     */
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(s_numCreated_++) {} // 使用原子变量保证序列号的唯一性和线程安全

    /**
     * @brief 执行定时器回调
     */
    void run() const
    {
        if (callback_)
        {
            callback_();
        }
    }

    // Getters
    Timestamp expiration() const { return expiration_; } // 获取到期时间
    bool repeat() const { return repeat_; }              // 是否为周期性定时器
    int64_t sequence() const { return sequence_; }       // 获取定时器序列号

    /**
     * @brief 重启定时器(用于周期性任务)
     * @param now 当前时间
     *
     * 如果是周期性定时器,将到期时间更新为当前时间加上间隔；否则设为无效时间戳
     */
    void restart(Timestamp now);

    /**
     * @brief 获取已创建的定时器总数(用于调试)
     */
    static int64_t numCreated() { return s_numCreated_; }

private:
    const TimerCallback callback_; // 定时器回调函数
    Timestamp expiration_;         // 到期时间
    const double interval_;        // 重复间隔(秒),如果>0,则为周期性定时器
    const bool repeat_;            // 是否为周期性定时器
    const int64_t sequence_;       // 定时器的唯一序列号

    static std::atomic<int64_t> s_numCreated_; // 全局原子计数器,用于生成序列号
};