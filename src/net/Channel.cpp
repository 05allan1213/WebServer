#include "Channel.h"

#include <sys/epoll.h>

#include "EventLoop.h"
#include "log/Log.h"

const int Channel::kNoneEvent = 0;                  // 无事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 读事件（包括普通读和带外数据）
const int Channel::kWriteEvent = EPOLLOUT;          // 写事件

/**
 * @brief 构造函数
 * @param loop 所属的EventLoop指针
 * @param fd 要管理的文件描述符
 * @details 初始化Channel对象，设置基本属性
 */
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

/**
 * @brief 析构函数
 * @details 自动从EventLoop中移除Channel，清理相关资源
 */
Channel::~Channel() {}

/**
 * @brief 绑定一个共享指针对象
 * @param obj 要绑定的共享指针对象
 * @details 通过weak_ptr机制，当上层对象被销毁时，Channel可以感知到并停止执行回调，
 * 避免悬空指针导致的未定义行为
 */
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * @brief 更新事件监听状态
 * @details 通知EventLoop更新Channel在Poller中的监听状态
 */
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

/**
 * @brief 从EventLoop中移除当前Channel对象
 * @details 这会停止对文件描述符的监听，并清理相关资源
 */
void Channel::remove() { loop_->removeChannel(this); }

/**
 * @brief 事件处理核心函数
 * @param receiveTime 事件发生的时间戳
 * @details 根据Poller返回的事件类型调用相应的回调函数。这是Channel的核心方法，
 * 由EventLoop在事件循环中调用
 */
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        // 如果绑定了上层对象，先检查对象是否还存在
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
        // 如果guard为空，说明上层对象已被销毁，不执行回调
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

/**
 * @brief 实际的事件分发逻辑
 * @param receiveTime 事件发生的时间戳
 * @details 对每种事件，必须先判断对应的回调函数对象是否有效（非空），然后再调用它。
 * 这是handleEvent的私有实现，增加了安全检查
 */
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    DLOG_INFO << "channel handleEvent revents:" << revents_;

    // 处理EPOLLHUP事件（对端关闭连接）
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    // 处理EPOLLERR事件（发生错误）
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    // 处理EPOLLIN事件（有数据可读）
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    // 处理EPOLLOUT事件（可写）
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}