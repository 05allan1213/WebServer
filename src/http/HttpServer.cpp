#include "HttpServer.h"
#include "net/NetworkConfig.h"
#include "base/ThreadPool.h"
#include "http/perf/PerfMetrics.h"
#include <any>
#include <algorithm>
#include "log/Log.h"
#include "SocketContext.h"

/**
 * @brief HttpServer构造函数
 * @param loop 事件循环指针
 * @param addr 监听地址
 * @param name 服务器名称
 * @param config 网络配置对象的共享指针
 * @param threadPool 业务线程池指针（可选）
 *
 * 初始化底层TcpServer，设置连接和消息回调。
 * 为每个连接分配SocketContext用于状态管理。
 */
HttpServer::HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name, std::shared_ptr<NetworkConfig> config, ThreadPool *threadPool)
    : server_(loop, addr, name, config), threadPool_(threadPool)
{
    DLOG_INFO << "HttpServer 构造: 监听地址=" << addr.toIpPort();
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // TcpServer内部会根据config设置线程数，这里不再需要setThreadNum
}

/**
 * @brief 启用HTTPS支持
 * @param certPath 证书文件路径
 * @param keyPath 私钥文件路径
 */
void HttpServer::enableSSL(const std::string &certPath, const std::string &keyPath)
{
    server_.enableSSL(certPath, keyPath);
}

/**
 * @brief 连接建立/断开回调
 * @param conn TCP连接指针
 *
 * 新连接建立时为其分配一个SocketContext实例，断开时记录日志。
 * 支持WebSocket连接的状态管理和清理。
 */
void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        PerfMetrics::instance().onTcpConnectionOpened();
        DLOG_INFO << "新连接建立: " << conn->name() << ", peer: " << conn->peerAddress().toIpPort();
        // 为新连接创建一个统一的SocketContext，初始状态为HTTP
        conn->setContext(std::make_shared<SocketContext>());
    }
    else
    {
        PerfMetrics::instance().onTcpConnectionClosed();
        DLOG_INFO << "连接断开: " << conn->name() << ", peer: " << conn->peerAddress().toIpPort();

        if (conn->getMutableContext()->has_value())
        {
            auto context = std::any_cast<std::shared_ptr<SocketContext>>(*conn->getMutableContext());
            // 如果是WebSocket连接，需要通知处理器连接已关闭
            if (context && context->state == SocketContext::WEBSOCKET && context->wsHandler)
            {
                context->wsHandler->onClose(conn);
            }
        }
    }
}

/**
 * @brief 消息接收回调
 * @param conn TCP连接指针
 * @param buf 接收缓冲区
 * @param recvTime 接收时间戳
 *
 * 根据连接状态分发到不同的解析器：
 * - HTTP状态：使用HttpParser解析HTTP请求
 * - WebSocket状态：使用WebSocketParser解析WebSocket帧
 */
void HttpServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp recvTime)
{
    auto context = std::any_cast<std::shared_ptr<SocketContext>>(*conn->getMutableContext());

    // 根据上下文的状态，分发到不同的解析器
    if (context->state == SocketContext::HTTP)
    {
        // --- 处理HTTP请求 ---
        if (!context->httpParser.parseRequest(buf))
        {
            // 解析失败，返回400错误并关闭连接
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
                // 文本或二进制消息，转发给WebSocket处理器
                context->wsHandler->onMessage(conn, payload);
                break;
            case WebSocketParser::PING:
                // 收到PING，自动回复PONG
                conn->sendWebSocket(payload, WebSocketParser::PONG);
                break;
            case WebSocketParser::CONNECTION_CLOSE:
                // 收到关闭帧，关闭连接
                conn->shutdown();
                break;
            default:
                break;
            }
        };

        // 循环解析WebSocket帧，直到缓冲区为空或解析出错
        while (buf->readableBytes() > 0)
        {
            auto result = context->wsParser.parse(buf, onFrame);
            if (result == WebSocketParser::INCOMPLETE)
                break; // 数据不完整，等待更多数据
            if (result == WebSocketParser::ERROR)
            {
                // 解析出错，关闭连接
                conn->shutdown();
                break;
            }
        }
    }
}

/**
 * @brief HTTP请求处理回调
 * @param conn TCP连接指针
 * @param req HTTP请求对象
 *
 * 根据请求内容生成HTTP响应，支持连接管理和WebSocket升级。
 * 如果配置了业务线程池，将请求处理分发到线程池，避免阻塞IO线程。
 * WebSocket升级请求必须在IO线程处理，避免竞态。
 *
 * 实现 per-connection 串行调度：同一连接的请求按顺序处理，避免响应乱序。
 */
void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    // WebSocket升级请求必须在IO线程处理，避免与IO线程竞态
    auto upgradeHeader = req.getHeader("Upgrade");
    // 大小写不敏感匹配 "websocket"
    bool isWebSocketUpgrade = false;
    if (upgradeHeader)
    {
        std::string value = *upgradeHeader;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        isWebSocketUpgrade = value.find("websocket") != std::string::npos;
    }

    // 如果有业务线程池且不是WebSocket升级，实现 per-connection 串行调度
    if (threadPool_ && !isWebSocketUpgrade)
    {
        auto context = std::any_cast<std::shared_ptr<SocketContext>>(*conn->getMutableContext());

        // 创建请求处理任务
        auto processTask = [this, conn, req]()
        {
            auto connPtr = conn;
            auto request = req;

            // 在业务线程处理请求
            const std::string &connection = request.getHeader("connection").value_or("");
            bool close;
            if (request.getVersion() == HttpRequest::Version::kHttp11)
            {
                close = (connection == "close");
            }
            else
            {
                close = (connection != "keep-alive");
            }
            HttpResponse response(close);

            if (httpCallback_)
            {
                httpCallback_(request, &response);
            }

            // 在IO线程中发送响应
            connPtr->getLoop()->runInLoop([this, connPtr, request, response]()
                                          {
                if (response.getStatusCode() == HttpResponse::k101SwitchingProtocols)
                {
                    Buffer buf;
                    response.appendToBuffer(&buf);
                    connPtr->send(buf.retrieveAllAsString());

                    // 标记请求处理完成
                    auto ctx = std::any_cast<std::shared_ptr<SocketContext>>(*connPtr->getMutableContext());
                    ctx->processingRequest = false;
                    return;
                }

                if (request.getMethod() == HttpRequest::Method::kHead)
                {
                    HttpResponse headResponse = response;
                    headResponse.setIncludeBody(false);
                    Buffer buf;
                    headResponse.appendToBuffer(&buf);
                    connPtr->send(buf.retrieveAllAsString());
                }
                else
                {
                    // 检查是否使用零拷贝发送文件
                    const auto &filePath = response.getFilePath();
                    if (filePath.has_value() && !filePath->empty())
                    {
                        // 标记正在发送文件
                        auto ctx = std::any_cast<std::shared_ptr<SocketContext>>(*connPtr->getMutableContext());
                        ctx->sendingFile = true;

                        // 先发送响应头
                        Buffer buf;
                        response.appendToBuffer(&buf);
                        connPtr->send(buf.retrieveAllAsString());

                        // 创建完成回调，在文件发送完成后标记请求完成并调度下一个请求
                        bool willClose = response.closeConnection();
                        auto completionCallback = [this, connPtr, willClose]() {
                            connPtr->getLoop()->runInLoop([this, connPtr, willClose]() {
                                auto ctx = std::any_cast<std::shared_ptr<SocketContext>>(*connPtr->getMutableContext());
                                ctx->processingRequest = false;
                                ctx->sendingFile = false;  // 清除文件发送标志

                                // 如果决定关闭连接，清空待处理队列
                                if (willClose)
                                {
                                    std::lock_guard<std::mutex> lock(ctx->taskMutex);
                                    while (!ctx->pendingTasks.empty())
                                    {
                                        ctx->pendingTasks.pop();
                                    }
                                    return;
                                }

                                SocketContext::PendingTask nextTask;
                                {
                                    std::lock_guard<std::mutex> lock(ctx->taskMutex);
                                    if (!ctx->pendingTasks.empty())
                                    {
                                        nextTask = std::move(ctx->pendingTasks.front());
                                        ctx->pendingTasks.pop();
                                        ctx->processingRequest = true;
                                    }
                                }

                                if (nextTask)
                                {
                                    if (!threadPool_->submit(std::move(nextTask)))
                                    {
                                        // 提交失败，复位状态并关闭连接
                                        DLOG_ERROR << "Failed to submit next task, closing connection";
                                        ctx->processingRequest = false;
                                        connPtr->shutdown();
                                    }
                                }
                            });
                        };

                        // 使用零拷贝发送文件，传入完成回调
                        connPtr->sendFile(*filePath, willClose, completionCallback);
                        return;
                    }

                    // 普通响应
                    Buffer buf;
                    response.appendToBuffer(&buf);
                    connPtr->send(buf.retrieveAllAsString());
                }

                if (response.closeConnection())
                {
                    connPtr->shutdown();
                }

                // 标记请求处理完成，检查是否有待处理的请求
                auto ctx = std::any_cast<std::shared_ptr<SocketContext>>(*connPtr->getMutableContext());
                ctx->processingRequest = false;

                // 如果决定关闭连接，清空待处理队列
                if (response.closeConnection())
                {
                    std::lock_guard<std::mutex> lock(ctx->taskMutex);
                    while (!ctx->pendingTasks.empty())
                    {
                        ctx->pendingTasks.pop();
                    }
                    return;
                }

                SocketContext::PendingTask nextTask;
                {
                    std::lock_guard<std::mutex> lock(ctx->taskMutex);
                    if (!ctx->pendingTasks.empty())
                    {
                        nextTask = std::move(ctx->pendingTasks.front());
                        ctx->pendingTasks.pop();
                        ctx->processingRequest = true;
                    }
                }

                // 如果有待处理的请求，提交到线程池
                if (nextTask)
                {
                    if (!threadPool_->submit(std::move(nextTask)))
                    {
                        // 提交失败，复位状态并关闭连接
                        DLOG_ERROR << "Failed to submit next task, closing connection";
                        ctx->processingRequest = false;
                        connPtr->shutdown();
                    }
                } });
        };

        // 检查连接状态：如果连接未连接（包括 disconnecting/disconnected），清理队列
        if (!conn->connected())
        {
            DLOG_WARN << "Connection not connected (state may be disconnecting/disconnected), dropping request";
            // 清空待处理队列
            std::lock_guard<std::mutex> lock(context->taskMutex);
            while (!context->pendingTasks.empty())
            {
                context->pendingTasks.pop();
            }
            context->processingRequest = false;
            context->sendingFile = false;
            return;
        }

        // 如果正在发送文件，新请求会被 processingRequest 机制自动排队
        // sendingFile 标志用于调试和状态追踪

        // 检查是否有请求正在处理中
        bool expected = false;
        if (context->processingRequest.compare_exchange_strong(expected, true))
        {
            // 没有请求在处理，直接提交
            if (!threadPool_->submit(std::move(processTask)))
            {
                // 提交失败，复位状态并返回503
                context->processingRequest = false;
                DLOG_ERROR << "ThreadPool queue full, returning 503";

                HttpResponse response(true);
                response.setStatusCode(HttpResponse::k503ServiceUnavailable);
                response.setStatusMessage("Service Unavailable");
                response.setContentType("text/plain");
                response.setBody("Server too busy, please try again later");

                Buffer buf;
                response.appendToBuffer(&buf);
                conn->send(buf.retrieveAllAsString());
                conn->shutdown();
            }
        }
        else
        {
            // 有请求在处理，加入队列
            std::lock_guard<std::mutex> lock(context->taskMutex);
            context->pendingTasks.push(std::move(processTask));
        }
        return;
    }

    // 没有线程池或WebSocket升级，直接在IO线程处理
    const std::string &connection = req.getHeader("connection").value_or("");
    bool close;
    if (req.getVersion() == HttpRequest::Version::kHttp11)
    {
        close = (connection == "close");
    }
    else
    {
        close = (connection != "keep-alive");
    }
    HttpResponse response(close);

    if (httpCallback_)
    {
        httpCallback_(req, &response);
    }

    if (response.getStatusCode() == HttpResponse::k101SwitchingProtocols)
    {
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(buf.retrieveAllAsString());
        return;
    }

    if (req.getMethod() == HttpRequest::Method::kHead)
    {
        response.setIncludeBody(false);
    }

    // 检查是否使用零拷贝发送文件
    const auto &filePath = response.getFilePath();
    if (filePath.has_value() && !filePath->empty())
    {
        // 先发送响应头
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(buf.retrieveAllAsString());

        // 使用零拷贝发送文件（IO线程直接处理，无需完成回调）
        conn->sendFile(*filePath, response.closeConnection(), nullptr);
        return;
    }

    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(buf.retrieveAllAsString());

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}
