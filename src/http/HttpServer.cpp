#include "HttpServer.h"
#include <any>
#include "log/Log.h"

/**
 * @brief HttpServer 构造函数
 * @param loop      事件循环指针
 * @param addr      监听地址
 * @param name      服务器名称
 * @param threadNum IO线程数
 *
 * 初始化底层TcpServer,设置连接和消息回调,配置线程数。
 */
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

/**
 * @brief 消息到达回调,驱动HTTP协议解析和业务处理
 * @param conn     TCP连接指针
 * @param buf      输入缓冲区
 * @param recvTime 接收时间戳
 *
 * 解析HTTP请求,若出错则返回400,否则调用onRequest处理业务。
 */
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
        DLOG_INFO << "HTTP请求解析完成: 连接=" << conn->name() << ", 请求路径=" << parser->request().getPath();
        onRequest(conn, parser->request());
        parser->reset();
    }
}

/**
 * @brief 业务处理回调,生成HTTP响应
 * @param conn TCP连接指针
 * @param req  解析后的HTTP请求
 *
 * 调用用户设置的httpCallback_生成响应,写入缓冲区并发送。
 * 若需要关闭连接则主动shutdown。
 */
void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    DLOG_INFO << "处理HTTP请求: 连接=" << conn->name() << ", 路径=" << req.getPath();
    const std::string &connection = req.getHeader("Connection").value_or("close");
    bool close = (connection == "close") ||
                 (req.getVersion() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");
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
