#pragma once

#include <unistd.h>
#include <sys/syscall.h>

/**
 * @brief 当前线程工具命名空间
 *
 * CurrentThread命名空间提供了获取当前线程ID的工具函数。
 * 主要功能：
 * - 缓存当前线程的TID（Thread ID）
 * - 提供高效的TID获取接口
 * - 使用线程局部存储确保线程安全
 *
 * 设计特点：
 * - 使用线程局部变量缓存TID，避免重复系统调用
 * - 使用编译器优化提示提高性能
 * - 提供内联函数减少函数调用开销
 * - 线程安全，每个线程都有独立的缓存
 *
 * 使用场景：
 * - 在EventLoop中检查线程一致性
 * - 需要获取当前线程ID时
 * - 线程调试和日志记录
 */
namespace CurrentThread
{
    // 每个线程都有一份独立的拷贝，用于缓存当前线程的TID
    extern __thread int t_cachedTid;

    // 通过系统调用获取当前线程的TID并缓存到线程局部变量中
    void cacheTid();

    /**
     * @brief 获取当前线程的TID
     * @return 当前线程的TID
     *
     * 内联函数，优先返回缓存的值，如果缓存为空则调用cacheTid()获取。
     * 使用__builtin_expect优化分支预测，提高性能。
     */
    inline int tid()
    {
        // __builtin_expect是编译器优化提示，告诉编译器t_cachedTid == 0这个分支可能性较小
        if (__builtin_expect(t_cachedTid == 0, 0))
        {               // 0表示false分支可能性小
            cacheTid(); // 如果还没缓存，调用函数去获取并缓存
        }
        return t_cachedTid; // 返回缓存的值
    }
}