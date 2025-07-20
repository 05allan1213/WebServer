#pragma once
#include "net/TcpServer.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <functional>

class HttpServer
{
public:
    using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, int threadNum);

    void setHttpCallback(const HttpCallback &cb) { httpCallback_ = cb; }
    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime);
    void onRequest(const TcpConnectionPtr &conn, const HttpRequest &req);

    TcpServer server_;
    HttpCallback httpCallback_;
};
