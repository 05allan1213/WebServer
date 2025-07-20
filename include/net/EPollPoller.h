#pragma once

#include <vector>
#include <sys/epoll.h>
#include "Poller.h"
#include "Timestamp.h"

/**
 * @brief epoll多路复用器的具体实现
 *
 * EPollPoller封装了Linux epoll的三大核心操作：
 * 1. epoll_create：创建epoll实例
 * 2. epoll_ctl：添加/修改/删除文件描述符的监听
 * 3. epoll_wait：等待事件发生
 *
 * epoll是Linux下高效的IO多路复用机制，相比select和poll有以下优势：
 * - 时间复杂度为O(1)，不受文件描述符数量影响
 * - 使用事件通知机制，避免轮询
 * - 支持边缘触发和水平触发模式
 *
 * 这个类实现了Poller的抽象接口，是网络库在Linux平台上的默认IO多路复用实现。
 */

/**
 * @brief epoll多路复用器的具体实现类
 *
 * 继承自Poller基类，提供基于epoll的IO多路复用功能。
 * 管理epoll文件描述符和事件列表，实现高效的IO事件监听。
 */
class EPollPoller : public Poller
{
public:
    /**
     * @brief 构造函数
     * @param loop 所属的EventLoop指针
     *
     * 创建epoll实例，初始化事件列表
     */
    EPollPoller(EventLoop *loop);

    /**
     * @brief 析构函数
     *
     * 关闭epoll文件描述符，清理资源
     */
    ~EPollPoller() override;

    /**
     * @brief 执行epoll_wait等待事件发生
     * @param timeoutMs 超时时间（毫秒），-1表示无限等待
     * @param activeChannels 输出参数，用于填充活跃的Channel列表
     * @return 返回事件发生的时间戳
     *
     * 调用epoll_wait系统调用，等待文件描述符上的事件发生。
     * 当有事件发生时，将对应的Channel添加到activeChannels中。
     */
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;

    /**
     * @brief 更新Channel在epoll中的监听状态
     * @param channel 要更新监听状态的Channel指针
     *
     * 根据Channel的事件监听状态，调用epoll_ctl添加、修改或删除监听。
     */
    void updateChannel(Channel *channel) override;

    /**
     * @brief 从epoll中移除Channel的监听
     * @param channel 要移除的Channel指针
     *
     * 调用epoll_ctl删除对指定文件描述符的监听。
     */
    void removeChannel(Channel *channel) override;

private:
    /**
     * @brief 填充活跃的Channel列表
     * @param numEvents epoll_wait返回的事件数量
     * @param activeChannels 输出参数，要填充的活跃Channel列表
     *
     * 遍历epoll_wait返回的事件数组，将对应的Channel添加到活跃列表中。
     */
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    /**
     * @brief 更新Channel在epoll中的监听
     * @param operation epoll_ctl的操作类型（EPOLL_CTL_ADD/MOD/DEL）
     * @param channel 要操作的Channel指针
     *
     * 调用epoll_ctl系统调用，执行具体的监听操作。
     */
    void update(int operation, Channel *channel);

private:
    /** @brief events_数组的初始长度，用于预分配内存 */
    static const int kInitEventListSize = 16;

    /** @brief epoll事件列表类型 */
    using Eventlist = std::vector<epoll_event>;

    int epollfd_;      /**< epoll文件描述符，由epoll_create创建 */
    Eventlist events_; /**< epoll_wait返回的就绪事件数组 */
};