#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    // 如果找到的记录存在且对应的Channel对象与传入的channel一致，则返回true。
    return it != channels_.end() && it->second == channel;
}