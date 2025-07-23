#include "HttpServer.h"
#include "net/NetworkConfig.h"
#include <any>
#include "log/Log.h"

/**
 * @brief HttpServer 构造函数
 * @param loop      事件循环指针
 * @param addr      监听地址
 * @param name      服务器名称
 * @param config    网络配置对象的共享指针
 *
 * 初始化底层TcpServer,设置连接和消息回调。
 */
HttpServer::HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, std::shared_ptr<NetworkConfig> config)
    : server_(loop, addr, name, config) // <-- 将 config 传递给 TcpServer
{
    DLOG_INFO << "HttpServer 构造: 监听地址=" << addr.toIpPort();
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // TcpServer 内部会根据 config 设置线程数，这里不再需要 setThreadNum
}

/**
 * @brief 连接建立/断开回调
 * @param conn TCP连接指针
 *
 * 新连接建立时为其分配一个HttpParser实例,断开时记录日志。
 */
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
    auto *parser = std::any_cast<HttpParser>(conn->getMutableContext());
    try
    {
        if (!parser->parseRequest(buf))
        {
            DLOG_WARN << "HTTP请求解析失败: 连接=" << conn->name();
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
            return;
        }
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "Exception during parseRequest for " << conn->name() << ": " << e.what();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
        return;
    }

    if (parser->gotAll())
    {
        onRequest(conn, parser->request());
        parser->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    const std::string &connection = req.getHeader("Connection").value_or("close");
    bool close = (connection == "close") ||
                 (req.getVersion() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);

    if (httpCallback_)
    {
        httpCallback_(req, &response);
    }

    const auto &filePath = response.getFilePath();
    if (filePath.has_value())
    {
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(buf.retrieveAllAsString());
        // 将关闭连接的决定传递下去
        conn->sendFile(filePath.value(), response.closeConnection());
    }
    else
    {
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(buf.retrieveAllAsString());
        // 传统方式发送完后，如果需要关闭，则调用 shutdown
        if (response.closeConnection())
        {
            conn->shutdown();
        }
    }
}