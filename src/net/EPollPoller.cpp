#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include "EPollPoller.h"
#include "Channel.h"
#include "log/Log.h"

// channel未添加到poller中(初始状态)
const int kNew = -1; // channel的成员index_ = -1
// channel已添加到poller中(epoll已监听)
const int kAdded = 1;
// channel从poller中删除(逻辑删除，epoll可能已移除监听)
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    // 检查 epoll 实例是否创建成功
    if (epollfd_ < 0)
    {
        DLOG_FATAL << "epoll_create1 error:" << errno;
    }
}

EPollPoller::~EPollPoller()
{
    // 关闭 epoll 实例的文件描述符
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 在高并发场景下，用LOG_DEBUG输出日志更为合理
    DLOG_INFO << "func=" << __FUNCTION__ << " => fd total count:" << channels_.size();
    // 调用 epoll_wait 等待事件发生
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    // 立刻保存 errno，防止后续操作（如日志、时间获取）修改它
    int saveErrno = errno;
    // 获取当前时间
    Timestamp now(Timestamp::now());

    if (numEvents > 0) // 有事件发生
    {
        DLOG_INFO << numEvents << " events happened";
        // 处理就绪事件，填充活跃事件列表
        fillActiveChannels(numEvents, activeChannels);
        // 如果活跃事件列表已满，则扩容
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) // 超时，没有事件发生
    {
        DLOG_DEBUG << __FUNCTION__ << " timeout!";
    }
    else // 出错
    {
        // 忽略被信号中断(EINTR)的情况
        if (saveErrno != EINTR)
        {
            // 恢复 errno，以便日志库能获取正确的错误码
            errno = saveErrno;
            DLOG_ERROR << "EPollPoller::poll() error!";
        }
    }
    // 返回事件大致发生的时间
    return now;
}

// Channel update/remove -> EventLoop updateChannel/removeChannel -> Poller update/removeChannel
// 更新 Channel 在 epoll 中的监听状态
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    DLOG_INFO << "func=" << __FUNCTION__ << " => fd=" << channel->fd() << " events=" << channel->events() << " index=" << index;

    if (index == kNew || index == kDeleted)
    {
        // 情况1: Channel 是全新的(kNew) 或 之前已被逻辑删除(kDeleted)
        // 这两种情况都需要将 Channel 添加到 epoll 监听中
        if (index == kNew)
        {
            // 如果是全新的，将其添加到 Poller 内部的 channels_ map 中
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        // 将 Channel 状态标记为已添加
        channel->set_index(kAdded);
        // 调用内部 update 函数，执行 epoll_ctl(ADD) 操作
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // 情况2: Channel 已经是 kAdded 状态，表示已在 epoll 中监听
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 检查 Channel 是否已不再关心任何事件
        {
            // 如果不关心任何事件，则从 epoll 中移除监听
            update(EPOLL_CTL_DEL, channel);
            // 将 Channel 状态标记为已删除 (逻辑删除)
            channel->set_index(kDeleted);
        }
        else
        {
            // 如果仍然关心事件 (可能事件类型已改变)，则修改 epoll 中的监听设置
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从 epoll 中移除对 Channel 的监听，并从 Poller 管理中删除。
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    // 从 Poller 的 channels_ map 中移除 fd->Channel* 的映射
    channels_.erase(fd);

    DLOG_INFO << "func=" << __FUNCTION__ << " => fd=" << fd;

    // 获取 Channel 当前状态
    int index = channel->index();
    if (index == kAdded)
    {
        // 如果 Channel 当前仍在 epoll 中监听 (kAdded)，则需要调用 epoll_ctl 将其移除
        update(EPOLL_CTL_DEL, channel);
    }
    // 将 Channel 状态设置为 kDeleted (表示已从Poller中删除)
    channel->set_index(kDeleted);
}

// 遍历 epoll_wait 返回的就绪事件，填充 activeChannels 列表
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        // 关键: 直接从 epoll_event.data.ptr 中高效获取关联的 Channel 指针
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        // 将实际发生的事件 (epoll_event.events) 设置到 Channel 的 revents_ 成员中
        channel->set_revents(events_[i].events);
        // 将活跃的 Channel 添加到 EventLoop 提供的 activeChannels 列表中
        activeChannels->push_back(channel);
    }
}

// 实际调用 epoll_ctl 来执行添加、修改或删除操作
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    int fd = channel->fd();
    // 设置关心的事件掩码
    event.events = channel->events();
    // 设置关联数据为文件描述符
    event.data.fd = fd;
    // 关键: 将 Channel 对象的指针存入 data.ptr，用于 poll 中快速获取
    event.data.ptr = channel;

    // 调用 epoll_ctl 系统调用
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        // 如果操作是删除，记录 Error 日志，通常可以容忍
        if (operation == EPOLL_CTL_DEL)
        {
            DLOG_ERROR << "epoll_ctl del error:" << errno;
        }
        else // 如果是添加或修改失败，则是严重错误，记录 Fatal 日志并终止
        {
            DLOG_FATAL << "epoll_ctl add/mod error:" << errno;
        }
    }
}