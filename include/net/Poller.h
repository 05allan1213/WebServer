#pragma once

/**
 * muduo库中多路事件分发器的核心IO复用模块
 */
#include <vector>
#include <unordered_map>
#include "base/noncopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给IO复用提供统一的接口
    // 执行 I/O 复用等待，并将活跃 Channel 填充到 activeChannels
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    // 添加或更新 Poller 中对某个 Channel (及其 fd) 的监听状态
    virtual void updateChannel(Channel *channel) = 0;
    // 从 Poller 中移除对某个 Channel (及其 fd) 的监听
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;
    // EventLoop通过此接口获取默认的IO接口复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // map的key: sockfd，value: sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;

    ChannelMap channels_; // 存储所有注册的Channel

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};