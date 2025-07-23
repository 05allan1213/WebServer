#pragma once

#include "net/TcpServer.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <functional>
#include <memory>

class NetworkConfig;

/**
 * @brief HTTP服务器类,基于TcpServer实现,负责HTTP协议的解析与业务分发
 */
class HttpServer
{
public:
    using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    /**
     * @brief 构造HTTP服务器
     * @param loop      事件循环指针
     * @param addr      监听地址
     * @param name      服务器名称
     * @param config    网络配置对象的共享指针
     */
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, std::shared_ptr<NetworkConfig> config);

    /**
     * @brief 设置HTTP业务回调
     * @param cb 用户自定义的回调函数
     */
    void setHttpCallback(const HttpCallback &cb) { httpCallback_ = cb; }

    /**
     * @brief 启动HTTP服务器
     */
    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime);
    void onRequest(const TcpConnectionPtr &conn, const HttpRequest &req);

    TcpServer server_;          // 底层TCP服务器,负责网络通信
    HttpCallback httpCallback_; // 用户设置的HTTP业务回调
};