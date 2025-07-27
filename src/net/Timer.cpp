#include "net/Timer.h"

// 在.cpp文件中对静态原子变量进行定义和初始化
std::atomic<int64_t> Timer::s_numCreated_(0);

/**
 * @brief 重启定时器
 * @param now 当前时间
 * @details 如果是周期性定时器，将到期时间更新为当前时间加上间隔；否则设为无效时间戳
 */
void Timer::restart(Timestamp now)
{
    if (repeat_)
    {
        // 如果是重复性定时器，则将到期时间更新为当前时间加上间隔
        expiration_ = addTime(now, interval_);
    }
    else
    {
        // 如果不是重复性定时器，则将其设置为一个无效时间戳，表示不再生效
        expiration_ = Timestamp();
    }
}