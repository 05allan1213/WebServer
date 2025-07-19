#pragma once

#include <vector>
#include <sys/epoll.h>
#include "Poller.h"
#include "Timestamp.h"

/**
 * EPollPoller封装了epoll的三大核心操作:
 * 1. epoll_create
 * 2. epoll_ctl   add/mod/del
 * 3. epoll_wait
 */

class EPollPoller : public Poller
{
public:
    // 构造函数:创建一个epoll实例
    EPollPoller(EventLoop *loop);
    // 析构函数:关闭epoll实例的文件描述符
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    // 调用epoll_wait等待事件发生
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    // 更新 Channel 在 epoll 中的监听状态
    void updateChannel(Channel *channel) override;
    // 从 epoll 中移除对 Channel 的监听
    void removeChannel(Channel *channel) override;

private:
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

private:
    static const int kInitEventListSize = 16; // events_ 数组的初始长度

    using Eventlist = std::vector<epoll_event>;

    int epollfd_;      // epoll文件描述符
    Eventlist events_; // epoll_wait返回的就绪事件
};