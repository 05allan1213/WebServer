#pragma once

#include "net/TcpServer.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <functional>

/**
 * @brief HTTP服务器类,基于TcpServer实现,负责HTTP协议的解析与业务分发
 *
 * 用法：
 * 1. 创建HttpServer对象,传入EventLoop、监听地址、名称、线程数
 * 2. 通过setHttpCallback设置业务处理回调
 * 3. 调用start()启动服务器
 *
 * 用户只需关注HttpCallback的实现即可,网络细节和协议解析已自动处理。
 */
class HttpServer
{
public:
    /**
     * @brief HTTP业务回调类型
     * 参数：const HttpRequest& 表示解析后的请求对象
     *      HttpResponse*      表示待填充的响应对象
     */
    using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    /**
     * @brief 构造HTTP服务器
     * @param loop      事件循环指针
     * @param addr      监听地址
     * @param name      服务器名称
     * @param threadNum IO线程数
     */
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, int threadNum);

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
    /**
     * @brief 连接建立/断开回调,负责为每个连接分配HttpParser
     * @param conn TCP连接指针
     */
    void onConnection(const TcpConnectionPtr &conn);
    /**
     * @brief 消息到达回调,驱动HTTP协议解析和业务处理
     * @param conn     TCP连接指针
     * @param buf      输入缓冲区
     * @param recvTime 接收时间戳
     */
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime);
    /**
     * @brief 业务处理回调,生成HTTP响应
     * @param conn TCP连接指针
     * @param req  解析后的HTTP请求
     */
    void onRequest(const TcpConnectionPtr &conn, const HttpRequest &req);

    TcpServer server_;          // 底层TCP服务器,负责网络通信
    HttpCallback httpCallback_; // 用户设置的HTTP业务回调
};
