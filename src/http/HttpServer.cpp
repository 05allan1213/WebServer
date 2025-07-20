#include "HttpServer.h"
#include <any>
#include "log/Log.h"

HttpServer::HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, int threadNum)
    : server_(loop, addr, name)
{
    DLOG_INFO << "HttpServer 构造: 线程数=" << threadNum << ", 监听地址=" << addr.toIpPort();
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    server_.setThreadNum(threadNum);
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        DLOG_INFO << "新连接建立: " << conn->name() << ", peer: " << conn->peerAddress().toIpPort();
        conn->setContext(HttpParser());
    }
    else
    {
        DLOG_INFO << "连接断开: " << conn->name() << ", peer: " << conn->peerAddress().toIpPort();
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime)
{
    DLOG_INFO << "收到消息: 连接=" << conn->name() << ", 数据长度=" << buf->readableBytes();
    auto *parser = std::any_cast<HttpParser>(conn->getMutableContext());
    if (!parser->parseRequest(buf))
    {
        DLOG_WARN << "HTTP请求解析失败: 连接=" << conn->name();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
        return;
    }
    if (parser->gotAll())
    {
        DLOG_INFO << "HTTP请求解析完成: 连接=" << conn->name() << ", 请求路径=" << parser->request().path();
        onRequest(conn, parser->request());
        parser->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    DLOG_INFO << "处理HTTP请求: 连接=" << conn->name() << ", 路径=" << req.path();
    const std::string &connection = req.getHeader("Connection").value_or("close");
    bool close = (connection == "close") ||
                 (req.version() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);
    if (httpCallback_)
        httpCallback_(req, &response);
    Buffer buf;
    response.appendToBuffer(&buf);
    DLOG_INFO << "发送HTTP响应: 连接=" << conn->name() << ", 响应长度=" << buf.readableBytes();
    conn->send(buf.retrieveAllAsString());
    if (response.closeConnection())
    {
        DLOG_INFO << "关闭连接: " << conn->name();
        conn->shutdown();
    }
}
