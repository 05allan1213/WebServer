#include <stdlib.h>
#include "Poller.h"
#include "EPollPoller.h"

Poller *Poller::newDefaultPoller(EventLoop *loop, const std::string &epollMode)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll实例
    }
    else
    {
        return new EPollPoller(loop, epollMode); // 生成epoll实例,支持ET/LT
    }
}