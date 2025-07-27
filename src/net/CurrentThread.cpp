#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0; // 线程局部变量，每个线程都有独立的拷贝

    /**
     * @brief 缓存当前线程的TID
     * @details 通过系统调用获取当前线程的TID并缓存到线程局部变量中
     */
    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            // 通过linux系统调用syscall，获取内核线程ID
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}