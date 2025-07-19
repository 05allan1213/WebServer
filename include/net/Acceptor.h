#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
   public:
    // 新连接回调函数
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    // 设置新连接回调函数
    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = cb; }
    // 检查当前Acceptor是否在监听
    bool listenning() const { return listenning_; }
    // 监听网络连接
    void listen();

   private:
    // 处理读取事件(有新连接)
    void handleRead();

   private:
    EventLoop* loop_;        // 指向 Acceptor 所属的 EventLoop(即baseLoop/mainLoop)
    Socket acceptSocket_;    // 封装 listen_fd 的 Socket 对象
    Channel acceptChannel_;  // 封装 listen_fd 的 Channel 对象
    NewConnectionCallback newConnectionCallback_;  // 当有新连接要执行的回调函数
    bool listenning_;                              // 当前 Acceptor 是否正在监听端口
};