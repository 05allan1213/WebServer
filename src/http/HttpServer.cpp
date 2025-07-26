#include "HttpServer.h"
#include "net/NetworkConfig.h"
#include <any>
#include "log/Log.h"
#include "SocketContext.h"

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
    : server_(loop, addr, name, config)
{
    DLOG_INFO << "HttpServer 构造: 监听地址=" << addr.toIpPort();
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // TcpServer 内部会根据 config 设置线程数，这里不再需要 setThreadNum
}

void HttpServer::enableSSL(const std::string &certPath, const std::string &keyPath)
{
    server_.enableSSL(certPath, keyPath);
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
        // 为新连接创建一个统一的SocketContext
        conn->setContext(std::make_shared<SocketContext>());
    }
    else
    {
        DLOG_INFO << "连接断开: " << conn->name() << ", peer: " << conn->peerAddress().toIpPort();
        auto context = std::any_cast<std::shared_ptr<SocketContext>>(conn->getContext());
        if (context && context->state == SocketContext::WEBSOCKET && context->wsHandler)
        {
            context->wsHandler->onClose(conn);
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime)
{
    auto context = std::any_cast<std::shared_ptr<SocketContext>>(conn->getMutableContext());

    // 根据上下文的状态，分发到不同的解析器
    if (context->state == SocketContext::HTTP)
    {
        // --- 处理HTTP请求 ---
        if (!context->httpParser.parseRequest(buf))
        {
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
            return;
        }

        if (context->httpParser.gotAll())
        {
            // 将TcpConnectionPtr存入HttpRequest的上下文中，传递给上层
            context->httpParser.getMutableRequest()->setContext(conn);
            onRequest(conn, context->httpParser.request());
            context->httpParser.reset();
        }
    }
    else // WEBSOCKET
    {
        // --- 处理WebSocket帧 ---
        auto onFrame = [&](WebSocketParser::Opcode opcode, const std::string &payload)
        {
            switch (opcode)
            {
            case WebSocketParser::TEXT_FRAME:
            case WebSocketParser::BINARY_FRAME:
                context->wsHandler->onMessage(conn, payload);
                break;
            case WebSocketParser::PING:
                conn->sendWebSocket(payload, WebSocketParser::PONG);
                break;
            case WebSocketParser::CONNECTION_CLOSE:
                conn->shutdown();
                break;
            default:
                break;
            }
        };

        while (buf->readableBytes() > 0)
        {
            auto result = context->wsParser.parse(buf, onFrame);
            if (result == WebSocketParser::INCOMPLETE)
                break;
            if (result == WebSocketParser::ERROR)
            {
                conn->shutdown();
                break;
            }
        }
    }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    const std::string &connection = req.getHeader("Connection").value_or("close");
    bool close = (connection == "close") || (req.getVersion() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);

    if (httpCallback_)
    {
        httpCallback_(req, &response);
    }

    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(buf.retrieveAllAsString());

    if (response.getStatusCode() != HttpResponse::k101SwitchingProtocols && response.closeConnection())
    {
        conn->shutdown();
    }
}