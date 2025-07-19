#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    // 声明线程局部变量
    extern __thread int t_cachedTid; // 线程局部变量，每个线程都有一份独立的拷贝

    // 缓存 TID
    void cacheTid();

    // 获取 TID
    inline int tid()
    {
        // __builtin_expect 是编译器优化提示，告诉编译器 t_cachedTid == 0 这个分支可能性较小
        if (__builtin_expect(t_cachedTid == 0, 0))
        {               // 0 表示 false 分支可能性小
            cacheTid(); // 如果还没缓存，调用函数去获取并缓存
        }
        return t_cachedTid; // 返回缓存的值
    }
}