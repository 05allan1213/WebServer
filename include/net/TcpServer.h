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
#include "log/Log.h"

/**
 * @brief TCP服务器类,提供TCP连接的管理和服务
 *
 * TcpServer是网络库的服务器端核心组件,负责：
 * - 监听指定端口的连接请求
 * - 管理所有已建立的TCP连接
 * - 分发连接事件到用户回调函数
 * - 支持多线程处理(主从Reactor模式)
 *
 * 设计特点：
 * - 采用主从Reactor模式：主Reactor负责接受新连接,从Reactor负责处理已连接socket的IO
 * - 支持线程池：可以创建多个IO线程处理连接
 * - 连接生命周期管理：自动管理连接的建立、断开和销毁
 * - 回调机制：提供丰富的回调接口供用户自定义处理逻辑
 * - 端口复用：支持SO_REUSEPORT选项
 */
class TcpServer : noncopyable
{
public:
    /** @brief 线程初始化回调函数类型 */
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    /**
     * @brief 端口复用选项枚举
     *
     * 用于控制是否启用SO_REUSEPORT端口复用选项,影响服务器的启动行为
     */
    enum Option // 用于控制是否启用 SO_REUSEPORT 端口复用选项
    {
        kNoReusePort, /**< 不启用端口复用 */
        kReusePort,   /**< 启用端口复用 */
    };

    /**
     * @brief 构造函数
     * @param loop 主EventLoop指针,负责接受新连接
     * @param listenAddr 监听地址
     * @param nameArg 服务器名称
     * @param option 端口复用选项,默认为kNoReusePort
     */
    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,
              Option option = kNoReusePort);

    /**
     * @brief 析构函数
     *
     * 清理所有连接资源,停止服务器
     */
    ~TcpServer();

    /**
     * @brief 设置线程初始化回调函数
     * @param cb 线程初始化回调函数
     *
     * 当IO线程启动时,会调用此回调函数进行初始化
     */
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }

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
     * @brief 设置IO线程池的大小
     * @param numThreads IO线程数量,0表示单线程模式
     *
     * 必须在start()之前调用,设置后不能修改
     */
    void setThreadNum(int numThreads);

    /**
     * @brief 启动服务器
     *
     * 开始监听连接请求,启动IO线程池(如果设置了的话)
     */
    void start();

private:
    /**
     * @brief 处理新连接
     * @param sockfd 新连接的socket文件描述符
     * @param peerAddr 对端地址
     *
     * 由Acceptor调用,创建TcpConnection对象并分发给IO线程
     */
    void newConnection(int sockfd, const InetAddress &peerAddr);

    /**
     * @brief 处理连接断开
     * @param conn 要断开的连接指针
     *
     * 从连接映射表中移除连接,并调用用户回调函数
     */
    void removeConnection(const TcpConnectionPtr &conn);

    /**
     * @brief 在EventLoop线程中处理连接断开
     * @param conn 要断开的连接指针
     *
     * 这是removeConnection的线程安全版本,确保在正确的线程中执行
     */
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    /** @brief 连接映射表类型,key为连接名称,value为连接指针 */
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // 主EventLoop,负责接受新连接

    const std::string ipPort_; // 服务器监听的IP:端口字符串
    const std::string name_;   // 服务器的名称,用于标识和日志

    std::unique_ptr<Acceptor> acceptor_; // 连接接受器,负责监听和接受新连接

    std::shared_ptr<EventLoopThreadPool>
        threadPool_; // IO线程池,处理已建立连接上的IO事件

    /** @brief 用户设置的回调函数 */
    ConnectionCallback connectionCallback_;       // 连接建立/断开回调函数
    MessageCallback messageCallback_;             // 消息(读事件)回调函数
    WriteCompleteCallback writeCompleteCallback_; // 写完成回调函数

    ThreadInitCallback threadInitCallback_; // 线程初始化回调函数

    std::atomic_int started_; // 服务器是否已启动的标志,原子变量保证线程安全

    int nextConnId_;            // 为新连接分配的ID计数器
    ConnectionMap connections_; // 存储当前服务器持有的所有活动TCP连接
};