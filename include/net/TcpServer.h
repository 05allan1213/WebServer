#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Acceptor.h"
#include "base/Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "base/noncopyable.h"

class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option // 用于控制是否启用 SO_REUSEPORT 端口复用选项
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    // 设置线程初始化回调
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    // 设置用户回调函数
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    // 设置EventLoopThreadPool中I/O线程(Sub Loop)的数量
    void setThreadNum(int numThreads);
    // 启动服务器
    void start();

private:
    // 处理新连接
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // 处理连接断开
    void removeConnection(const TcpConnectionPtr &conn);
    // 处理连接断开
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseLoop/mainLoop

    const std::string ipPort_; // ip:port
    const std::string name_;   // 服务器的名称

    std::unique_ptr<Acceptor> acceptor_; // 指向Acceptor对象的智能指针，用于监听新连接事件

    std::shared_ptr<EventLoopThreadPool>
        threadPool_; // 指向EventLoopThreadPool对象的智能指针，处理已建立连接上的 I/O 事件

    ConnectionCallback connectionCallback_;       // 用户设置的连接回调函数
    MessageCallback messageCallback_;             // 用户设置的消息（读事件）回调函数
    WriteCompleteCallback writeCompleteCallback_; // 用户设置的写完成回调函数

    ThreadInitCallback threadInitCallback_; // 用户设置的线程初始化回调函数

    std::atomic_int started_; // 服务器是否启动的标志

    int nextConnId_;            // 为新连接分配的ID
    ConnectionMap connections_; // 存储当前服务器持有的所有活动 TCP 连接
};