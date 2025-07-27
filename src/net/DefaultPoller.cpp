#include <stdlib.h>
#include "Poller.h"
#include "EPollPoller.h"

/**
 * @brief 创建默认的Poller实例
 * @param loop 所属的EventLoop指针
 * @param epollMode epoll模式，支持ET/LT
 * @return Poller指针，根据环境变量决定返回poll或epoll实例
 * @details 通过环境变量MUDUO_USE_POLL控制使用poll还是epoll，默认使用epoll
 */
Poller *Poller::newDefaultPoller(EventLoop *loop, const std::string &epollMode)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll实例,暂未实现,返回空指针
    }
    else
    {
        return new EPollPoller(loop, epollMode); // 生成epoll实例,支持ET/LT
    }
}