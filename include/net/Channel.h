#pragma once

#include <functional>
#include <memory>
#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;

/**
 * Channel 可理解为通道，封装了sockfd和事件，如EPOLLIN，EPOLLOUT等
 *                      还绑定了Poller返回的具体事件
 */

class Channel : noncopyable
{
public:
    // 读写事件回调函数
    using EventCallback = std::function<void()>;
    // 只读事件回调函数
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 事件处理核心函数，根据接收到的时间戳调用相应的回调函数。
    void handleEvent(Timestamp receiveTime);

    // 设置相应事件的回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 绑定一个共享指针对象，确保Channel对象在手动移除后不会继续执行回调
    void tie(const std::shared_ptr<void> &);

    // 获取fd
    int fd() const { return fd_; }
    // 设置fd的监听事件
    int events() const { return events_; }
    // 设置已发生的事件集合
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态，即对不同事件的监听
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件监听状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // 获取索引值
    int index() { return index_; }
    // 设置索引值
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    // 获取所属的EventLoop对象
    EventLoop *ownerLoop() { return loop_; }
    // 从EventLoop中移除当前Channel对象
    void remove();

private:
    // 更新事件监听状态
    void update();
    /**
     * 实际的事件分发逻辑
     * 对每种事件，必须先判断对应的回调函数对象是否有效（非空），然后再调用它。
     */
    void handleEventWithGuard(Timestamp receiveTime);

private:
    EventLoop *loop_; // 事件循环
    int fd_;          // Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // Poller实际返回的具体事件
    int index_;       // 供Poller使用，标记Channel在Poller中的状态

    // 表示不同的事件类型
    static const int kNoneEvent;  // 无事件
    static const int kReadEvent;  // 读事件
    static const int kWriteEvent; // 写事件

    std::weak_ptr<void> tie_; // 弱指针，用于“绑定”上层对象
    bool tied_;               // 标记channel是否绑定了上层对象

    // 具体事件的回调操作
    ReadEventCallback readCallback_; // 读事件
    EventCallback writeCallback_;    // 写事件
    EventCallback closeCallback_;    // 关闭事件
    EventCallback errorCallback_;    // 错误事件
};