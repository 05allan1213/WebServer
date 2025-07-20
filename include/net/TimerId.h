#pragma once

#include <cstdint>
#include "net/Timer.h"

/**
 * @brief 定时器的外部标识符
 *
 * 这是一个对用户不透明的句柄,用于取消定时器。
 * 它通过持有 Timer 指针和其序列号来唯一标识一个定时器实例。
 * 这样做是为了处理“定时器A的回调函数中取消了定时器B,而B的地址可能被新创建的定时器C复用”的ABA问题。
 *
 * 设计说明：
 * - TimerId 只做唯一标识,不拥有 Timer 对象的所有权
 * - 结合 TimerQueue 的 activeTimers_,可安全高效地取消定时器
 */
class TimerId
{
public:
    // 默认构造一个无效的 TimerId
    TimerId() : timer_(nullptr), sequence_(0) {}

    // 构造一个有效的 TimerId
    TimerId(Timer *timer, int64_t seq) : timer_(timer), sequence_(seq) {}

    // 允许默认的拷贝构造、赋值和析构

    // 让 TimerQueue 成为友元,以便它可以访问私有成员
    friend class TimerQueue;

private:
    Timer *timer_;     // 指向 Timer 对象(不拥有所有权)
    int64_t sequence_; // Timer 对象的序列号
};