#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "base/Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "base/noncopyable.h"

class Channel;
class EventLoop;
class Socket;

// TcpConnection 代表一个TCP连接
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

    // 获取相关信息的接口
    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    // 判断是否连接
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    // 向对端发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown(); // 关闭写端

    // 设置回调函数
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    // 连接建立和销毁
    void connectEstablished(); // 连接建立后调用，注册Channel到Poller
    void connectDestroyed();   // 连接销毁前调用，从Poller移除Channel

private:
    enum State // 连接状态枚举
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    // 设置连接状态
    void setState(State state) { state_ = state; }

    // Channel的回调处理函数
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    // 在所属的loop中执行发送/关闭操作
    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();

private:
    EventLoop *loop_;        // 这里不是baseLoop，因为TcpConnection是在subLoop里面管理的
    const std::string name_; // 连接名称
    std::atomic_int state_;  // 连接状态
    bool reading_;           // 是否正在读取数据(用于控制Channel的读事件关注)

    std::unique_ptr<Socket> socket_;   // 封装connfd
    std::unique_ptr<Channel> channel_; // 封装connfd对应的事件

    const InetAddress localAddr_; // 本地地址
    const InetAddress peerAddr_;  // 对端地址

    // 用户设置的回调函数
    ConnectionCallback connectionCallback_;       // 连接建立/断开回调
    MessageCallback messageCallback_;             // 消息到达回调
    WriteCompleteCallback writeCompleteCallback_; // 数据发送完毕回调 (outputBuffer 清空时)
    HighWaterMarkCallback highWaterMarkCallback_; // 输出缓冲区高水位回调
    CloseCallback closeCallback_;                 // 连接关闭回调 (通知 TCPServer)
    size_t highWaterMark_;                        // 高水位阈值

    // 数据缓冲区
    Buffer inputBuffer_;  // 接收缓冲区
    Buffer outputBuffer_; // 发送缓冲区
};