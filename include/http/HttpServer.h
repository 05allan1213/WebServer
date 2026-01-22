#pragma once

#include "net/TcpServer.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <functional>
#include <memory>

class NetworkConfig;
class ThreadPool;

/**
 * @brief HTTP服务器类，基于TcpServer实现，负责HTTP协议的解析与业务分发
 *
 * 提供HTTP协议层的封装，包括请求解析、响应生成、连接管理等。
 * 支持HTTPS和HTTP/1.1协议。
 */
class HttpServer
{
public:
    using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    /**
     * @brief 构造HTTP服务器
     * @param loop 事件循环指针
     * @param addr 监听地址
     * @param name 服务器名称
     * @param config 网络配置对象的共享指针
     * @param threadPool 业务线程池指针（可选）
     */
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, std::shared_ptr<NetworkConfig> config, ThreadPool *threadPool = nullptr);

    /**
     * @brief 启用HTTPS
     * @param certPath 证书路径
     * @param keyPath 密钥路径
     */
    void enableSSL(const std::string &certPath, const std::string &keyPath);

    /**
     * @brief 设置HTTP业务回调
     * @param cb 用户自定义的回调函数
     */
    void setHttpCallback(const HttpCallback &cb)
    {
        httpCallback_ = cb;
    }

    /**
     * @brief 启动HTTP服务器
     */
    void start() { server_.start(); }

private:
    /**
     * @brief 连接建立回调
     * @param conn TCP连接对象
     */
    void onConnection(const TcpConnectionPtr &conn);

    /**
     * @brief 消息接收回调
     * @param conn TCP连接对象
     * @param buf 接收缓冲区
     * @param recvTime 接收时间戳
     */
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime);

    /**
     * @brief HTTP请求处理回调
     * @param conn TCP连接对象
     * @param req HTTP请求对象
     */
    void onRequest(const TcpConnectionPtr &conn, const HttpRequest &req);

    TcpServer server_;          // 底层TCP服务器，负责网络通信
    HttpCallback httpCallback_; // 用户设置的HTTP业务回调
    ThreadPool *threadPool_;    // 业务线程池指针（可选）
};