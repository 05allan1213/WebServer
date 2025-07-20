#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

class EventLoop;
class InetAddress;

/**
 * @brief 连接接收器,负责监听新连接
 *
 * Acceptor用于封装服务器端监听socket的创建、绑定、监听和新连接接收。
 * 主要功能：
 * - 封装listen_fd的Socket和Channel
 * - 负责监听指定端口的连接请求
 * - 当有新连接到来时,回调用户设置的NewConnectionCallback
 *
 * 设计特点：
 * - 只负责新连接的接收,不处理已连接socket的IO
 * - 支持端口复用(SO_REUSEPORT)
 * - 与EventLoop配合,实现主从Reactor模式
 */
class Acceptor : noncopyable
{
public:
    /**
     * @brief 新连接回调函数类型
     * @param sockfd 新连接的socket文件描述符
     * @param peerAddr 对端地址
     *
     * 当有新连接到来时调用,用户可在回调中创建TcpConnection对象
     */
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    /**
     * @brief 构造函数
     * @param loop 所属的EventLoop指针
     * @param listenAddr 监听地址
     * @param reuseport 是否启用端口复用
     *
     * 创建监听socket并绑定到指定地址,初始化Channel
     */
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);

    /**
     * @brief 析构函数
     *
     * 关闭监听socket,清理资源
     */
    ~Acceptor();

    /**
     * @brief 设置新连接回调函数
     * @param cb 新连接回调函数
     */
    void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }

    /**
     * @brief 检查当前Acceptor是否在监听
     * @return 如果正在监听返回true,否则返回false
     */
    bool listenning() const { return listenning_; }

    /**
     * @brief 开始监听网络连接
     *
     * 调用listen系统调用,开始监听端口
     */
    void listen();

private:
    /**
     * @brief 处理读事件(有新连接到来)
     *
     * 由EventLoop回调,接收新连接并调用用户设置的回调函数
     */
    void handleRead();

private:
    EventLoop *loop_;                             // 指向Acceptor所属的EventLoop(即baseLoop/mainLoop)
    Socket acceptSocket_;                         // 封装listen_fd的Socket对象
    Channel acceptChannel_;                       // 封装listen_fd的Channel对象
    NewConnectionCallback newConnectionCallback_; // 新连接到来时的回调函数
    bool listenning_;                             // 当前Acceptor是否正在监听端口
};