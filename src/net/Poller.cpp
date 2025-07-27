#include "Poller.h"
#include "Channel.h"

/**
 * @brief 构造函数
 * @param loop 所属的EventLoop指针
 * @details 初始化Poller对象，设置所属的EventLoop
 */
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

/**
 * @brief 检查Channel是否在Poller中注册
 * @param channel 要检查的Channel指针
 * @return 如果Channel已注册返回true，否则返回false
 * @details 通过文件描述符查找Channel，并验证Channel对象是否一致
 */
bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    // 如果找到的记录存在且对应的Channel对象与传入的channel一致,则返回true。
    return it != channels_.end() && it->second == channel;
}