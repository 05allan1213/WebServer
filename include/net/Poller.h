#pragma once

/**
 * @brief IO多路复用器的抽象基类
 *
 * Poller是网络库中多路事件分发器的核心IO复用模块，提供了统一的接口来监听多个文件描述符的事件。
 * 它封装了不同平台的IO多路复用机制（如Linux的epoll、BSD的kqueue等），
 * 为上层提供统一的接口。
 *
 * 主要功能：
 * - 监听文件描述符的IO事件
 * - 管理Channel的注册和移除
 * - 返回活跃的Channel列表
 * - 提供事件超时机制
 *
 * 设计模式：
 * - 采用工厂模式创建具体的Poller实现
 * - 使用虚函数提供多态接口
 * - 通过ChannelMap管理所有注册的Channel
 */
#include <vector>
#include <unordered_map>
#include "base/noncopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;

/**
 * @brief IO多路复用器的抽象基类
 *
 * 定义了IO多路复用的统一接口，具体的实现由子类提供（如EPollPoller）。
 * 每个Poller对象都隶属于一个EventLoop，负责监听该EventLoop管理的所有Channel。
 */
class Poller : noncopyable
{
public:
    /** @brief 活跃Channel列表类型 */
    using ChannelList = std::vector<Channel *>;

    /**
     * @brief 构造函数
     * @param loop 所属的EventLoop指针
     */
    Poller(EventLoop *loop);

    /**
     * @brief 虚析构函数
     */
    virtual ~Poller() = default;

    /**
     * @brief 执行IO多路复用等待
     * @param timeoutMs 超时时间（毫秒），-1表示无限等待
     * @param activeChannels 输出参数，用于填充活跃的Channel列表
     * @return 返回事件发生的时间戳
     *
     * 这是Poller的核心方法，会阻塞等待直到有事件发生或超时。
     * 当有事件发生时，将对应的Channel添加到activeChannels中。
     */
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

    /**
     * @brief 添加或更新Channel的监听状态
     * @param channel 要更新监听状态的Channel指针
     *
     * 当Channel的事件监听状态发生变化时调用，更新Poller内部的监听设置。
     */
    virtual void updateChannel(Channel *channel) = 0;

    /**
     * @brief 从Poller中移除Channel
     * @param channel 要移除的Channel指针
     *
     * 当Channel不再需要监听时调用，从Poller中移除该Channel。
     */
    virtual void removeChannel(Channel *channel) = 0;

    /**
     * @brief 检查Channel是否在当前Poller中注册
     * @param channel 要检查的Channel指针
     * @return 如果Channel已注册返回true，否则返回false
     */
    bool hasChannel(Channel *channel) const;

    /**
     * @brief 创建默认的Poller实现
     * @param loop 所属的EventLoop指针
     * @param epollMode epoll触发模式（"ET"/"LT"），默认LT
     * @return 返回新创建的Poller指针
     *
     * 工厂方法，根据当前平台选择最合适的IO多路复用实现。
     * 在Linux平台上通常返回EPollPoller。
     */
    static Poller *newDefaultPoller(EventLoop *loop, const std::string &epollMode = "LT");

protected:
    /** @brief Channel映射表类型，key为文件描述符，value为对应的Channel指针 */
    using ChannelMap = std::unordered_map<int, Channel *>;

    ChannelMap channels_; /**< 存储所有注册的Channel，用于快速查找和管理 */

private:
    EventLoop *ownerLoop_; /**< 定义Poller所属的事件循环EventLoop */
};