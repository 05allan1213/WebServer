#pragma once

#include <functional>
#include <memory>
#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;

/**
 * @brief Channel类，封装文件描述符和事件处理
 *
 * Channel可以理解为事件通道，封装了socket文件描述符和对应的IO事件（如EPOLLIN、EPOLLOUT等）。
 * 它是EventLoop和Poller之间的桥梁，负责：
 * - 管理文件描述符的事件监听状态
 * - 绑定具体事件的处理回调函数
 * - 处理Poller返回的事件并分发到相应的回调函数
 *
 * 每个Channel对象都隶属于一个EventLoop，采用"one loop per thread"的设计模式。
 * Channel的生命周期由上层对象（如TcpConnection）管理，通过tie机制确保安全。
 */
class Channel : noncopyable
{
public:
    /** @brief 读写事件回调函数类型 */
    using EventCallback = std::function<void()>;
    /** @brief 只读事件回调函数类型，包含接收时间戳 */
    using ReadEventCallback = std::function<void(Timestamp)>;

    /**
     * @brief 构造函数
     * @param loop 所属的EventLoop指针
     * @param fd 要管理的文件描述符
     */
    Channel(EventLoop *loop, int fd);

    /**
     * @brief 析构函数
     *
     * 自动从EventLoop中移除Channel，清理相关资源
     */
    ~Channel();

    /**
     * @brief 事件处理核心函数
     * @param receiveTime 事件发生的时间戳
     *
     * 根据Poller返回的事件类型调用相应的回调函数。这是Channel的核心方法，
     * 由EventLoop在事件循环中调用。
     */
    void handleEvent(Timestamp receiveTime);

    /**
     * @brief 设置读事件回调函数
     * @param cb 读事件回调函数
     */
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }

    /**
     * @brief 设置写事件回调函数
     * @param cb 写事件回调函数
     */
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }

    /**
     * @brief 设置连接关闭事件回调函数
     * @param cb 关闭事件回调函数
     */
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

    /**
     * @brief 设置错误事件回调函数
     * @param cb 错误事件回调函数
     */
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /**
     * @brief 绑定一个共享指针对象，确保Channel对象在手动移除后不会继续执行回调
     * @param obj 要绑定的共享指针对象
     *
     * 通过weak_ptr机制，当上层对象被销毁时，Channel可以感知到并停止执行回调，
     * 避免悬空指针导致的未定义行为。
     */
    void tie(const std::shared_ptr<void> &obj);

    /**
     * @brief 获取管理的文件描述符
     * @return 文件描述符
     */
    int fd() const { return fd_; }

    /**
     * @brief 获取当前监听的事件类型
     * @return 事件类型掩码
     */
    int events() const { return events_; }

    /**
     * @brief 设置Poller返回的事件类型
     * @param revt Poller返回的事件类型掩码
     */
    void set_revents(int revt) { revents_ = revt; }

    /**
     * @brief 启用读事件监听
     *
     * 将读事件添加到监听集合中，并通知Poller更新监听状态
     */
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }

    /**
     * @brief 禁用读事件监听
     *
     * 从监听集合中移除读事件，并通知Poller更新监听状态
     */
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }

    /**
     * @brief 启用写事件监听
     *
     * 将写事件添加到监听集合中，并通知Poller更新监听状态
     */
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }

    /**
     * @brief 禁用写事件监听
     *
     * 从监听集合中移除写事件，并通知Poller更新监听状态
     */
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }

    /**
     * @brief 禁用所有事件监听
     *
     * 清除所有监听事件，并通知Poller更新监听状态
     */
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    /**
     * @brief 检查是否没有监听任何事件
     * @return 如果没有监听事件返回true，否则返回false
     */
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    /**
     * @brief 检查是否正在监听写事件
     * @return 如果监听写事件返回true，否则返回false
     */
    bool isWriting() const { return events_ & kWriteEvent; }

    /**
     * @brief 检查是否正在监听读事件
     * @return 如果监听读事件返回true，否则返回false
     */
    bool isReading() const { return events_ & kReadEvent; }

    /**
     * @brief 获取Channel在Poller中的索引值
     * @return 索引值
     */
    int index() { return index_; }

    /**
     * @brief 设置Channel在Poller中的索引值
     * @param idx 新的索引值
     */
    void set_index(int idx) { index_ = idx; }

    /**
     * @brief 获取所属的EventLoop对象
     * @return EventLoop指针
     */
    EventLoop *ownerLoop() { return loop_; }

    /**
     * @brief 从EventLoop中移除当前Channel对象
     *
     * 这会停止对文件描述符的监听，并清理相关资源
     */
    void remove();

private:
    /**
     * @brief 更新事件监听状态
     *
     * 通知EventLoop更新Channel在Poller中的监听状态
     */
    void update();

    /**
     * @brief 实际的事件分发逻辑
     * @param receiveTime 事件发生的时间戳
     *
     * 对每种事件，必须先判断对应的回调函数对象是否有效（非空），然后再调用它。
     * 这是handleEvent的私有实现，增加了安全检查。
     */
    void handleEventWithGuard(Timestamp receiveTime);

private:
    EventLoop *loop_; /**< 所属的事件循环 */
    int fd_;          /**< Poller监听的文件描述符 */
    int events_;      /**< 注册的感兴趣事件类型 */
    int revents_;     /**< Poller实际返回的事件类型 */
    int index_;       /**< 供Poller使用，标记Channel在Poller中的状态 */

    /** @brief 表示不同的事件类型 */
    static const int kNoneEvent;  /**< 无事件 */
    static const int kReadEvent;  /**< 读事件 */
    static const int kWriteEvent; /**< 写事件 */

    std::weak_ptr<void> tie_; /**< 弱指针，用于"绑定"上层对象，防止悬空指针 */
    bool tied_;               /**< 标记channel是否绑定了上层对象 */

    /** @brief 具体事件的回调操作 */
    ReadEventCallback readCallback_; /**< 读事件回调函数 */
    EventCallback writeCallback_;    /**< 写事件回调函数 */
    EventCallback closeCallback_;    /**< 关闭事件回调函数 */
    EventCallback errorCallback_;    /**< 错误事件回调函数 */
};