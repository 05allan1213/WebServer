#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <any>

#include "base/Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "base/noncopyable.h"
#include "net/TimerId.h"

class Channel;
class EventLoop;
class Socket;

/**
 * @brief TCP连接类,代表一个TCP连接
 *
 * TcpConnection封装了一个完整的TCP连接,包括：
 * - 连接的生命周期管理(建立、断开、销毁)
 * - 数据的收发处理(输入输出缓冲区)
 * - 各种事件回调的处理(连接、消息、写完成等)
 * - 连接状态的管理和转换
 *
 * 每个TcpConnection对象都隶属于一个EventLoop(通常是subLoop),
 * 采用智能指针管理生命周期,支持enable_shared_from_this。
 *
 * 设计特点：
 * - 线程安全：所有操作都在EventLoop线程中执行
 * - 缓冲区管理：使用Buffer类管理输入输出数据
 * - 高水位控制：防止发送缓冲区无限增长
 * - 优雅关闭：支持shutdown机制
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    /**
     * @brief 构造函数
     * @param loop 所属的EventLoop指针
     * @param nameArg 连接名称
     * @param sockfd 已连接的socket文件描述符
     * @param localAddr 本地地址
     * @param peerAddr 对端地址
     */
    TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);

    /**
     * @brief 析构函数
     *
     * 清理连接资源,关闭socket,移除Channel
     */
    ~TcpConnection();

    /**
     * @brief 获取所属的EventLoop
     * @return EventLoop指针
     */
    EventLoop *getLoop() const { return loop_; }

    /**
     * @brief 获取连接名称
     * @return 连接名称字符串
     */
    const std::string &name() const { return name_; }

    /**
     * @brief 获取本地地址
     * @return 本地地址对象
     */
    const InetAddress &localAddress() const { return localAddr_; }

    /**
     * @brief 获取对端地址
     * @return 对端地址对象
     */
    const InetAddress &peerAddress() const { return peerAddr_; }

    /**
     * @brief 检查连接是否已建立
     * @return 如果连接已建立返回true,否则返回false
     */
    bool connected() const { return state_ == kConnected; }

    /**
     * @brief 检查连接是否已断开
     * @return 如果连接已断开返回true,否则返回false
     */
    bool disconnected() const { return state_ == kDisconnected; }

    /**
     * @brief 向对端发送数据
     * @param buf 要发送的数据字符串
     *
     * 如果当前线程是EventLoop线程,直接发送；否则将发送操作加入队列
     */
    void send(const std::string &buf);

    /**
     * @brief 关闭连接
     *
     * 关闭写端,停止发送数据,但保持读端开放
     */
    void shutdown(); // 关闭写端

    /**
     * @brief 设置连接建立/断开回调函数
     * @param cb 连接回调函数
     */
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }

    /**
     * @brief 设置消息到达回调函数
     * @param cb 消息回调函数
     */
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

    /**
     * @brief 设置写完成回调函数
     * @param cb 写完成回调函数
     */
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    /**
     * @brief 设置高水位回调函数
     * @param cb 高水位回调函数
     * @param highWaterMark 高水位阈值
     */
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    /**
     * @brief 设置连接关闭回调函数
     * @param cb 关闭回调函数
     */
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    /**
     * @brief 连接建立后调用,注册Channel到Poller
     *
     * 由TcpServer在建立新连接时调用,设置Channel的回调函数并启用读事件监听
     */
    void connectEstablished(); // 连接建立后调用,注册Channel到Poller

    /**
     * @brief 连接销毁前调用,从Poller移除Channel
     *
     * 由TcpServer在销毁连接时调用,清理Channel资源
     */
    void connectDestroyed(); // 连接销毁前调用,从Poller移除Channel

    // 上下文存取接口
    void setContext(const std::any &context) { context_ = context; }
    std::any *getMutableContext() { return &context_; }
    const std::any &getContext() const { return context_; }

private:
    /**
     * @brief 连接状态枚举
     */
    enum State // 连接状态枚举
    {
        kDisconnected, /**< 已断开连接 */
        kConnecting,   /**< 正在连接中 */
        kConnected,    /**< 已连接 */
        kDisconnecting /**< 正在断开连接 */
    };

    /**
     * @brief 设置连接状态
     * @param state 新的连接状态
     */
    void setState(State state) { state_ = state; }

    /**
     * @brief 处理读事件
     * @param receiveTime 事件发生的时间戳
     *
     * 从socket读取数据到输入缓冲区,并调用用户的消息回调函数
     */
    void handleRead(Timestamp receiveTime);

    /**
     * @brief 处理写事件
     *
     * 将输出缓冲区的数据写入socket,处理写完成回调
     */
    void handleWrite();

    /**
     * @brief 处理连接关闭事件
     *
     * 处理对端关闭连接的情况,调用相应的回调函数
     */
    void handleClose();

    /**
     * @brief 处理错误事件
     *
     * 处理socket错误,记录错误日志
     */
    void handleError();

    /**
     * @brief 在所属的EventLoop中执行发送操作
     * @param data 要发送的数据指针
     * @param len 数据长度
     *
     * 这是send方法的实际实现,在EventLoop线程中执行
     */
    void sendInLoop(const void *data, size_t len);

    /**
     * @brief 在所属的EventLoop中执行关闭操作
     *
     * 这是shutdown方法的实际实现,在EventLoop线程中执行
     */
    void shutdownInLoop();

private:
    EventLoop *loop_;        // 所属的EventLoop,通常是subLoop
    const std::string name_; // 连接名称,用于标识和日志
    std::atomic_int state_;  // 连接状态,原子变量保证线程安全
    bool reading_;           // 是否正在读取数据,用于控制Channel的读事件关注

    std::unique_ptr<Socket> socket_;   // 封装已连接的socket文件描述符
    std::unique_ptr<Channel> channel_; // 封装socket对应的事件Channel

    const InetAddress localAddr_; // 本地地址信息
    const InetAddress peerAddr_;  // 对端地址信息

    // 用户设置的回调函数
    ConnectionCallback connectionCallback_;       // 连接建立/断开回调
    MessageCallback messageCallback_;             // 消息到达回调
    WriteCompleteCallback writeCompleteCallback_; // 数据发送完毕回调(outputBuffer清空时)
    HighWaterMarkCallback highWaterMarkCallback_; // 输出缓冲区高水位回调
    CloseCallback closeCallback_;                 // 连接关闭回调(通知TcpServer)
    size_t highWaterMark_;                        // 高水位阈值,防止发送缓冲区无限增长
    TimerId idleTimerId_;                         // 空闲超时定时器ID

    // 数据缓冲区
    Buffer inputBuffer_;  // 接收缓冲区,存储从socket读取的数据
    Buffer outputBuffer_; // 发送缓冲区,存储待发送到socket的数据
    std::any context_;
};