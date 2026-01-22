#pragma once

#include "websocket/WebSocketHandler.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include <memory>

/**
 * @brief [DEMO] WebSocket Echo 处理器
 * @details 演示用途：将收到的消息原样返回
 */
class EchoWebSocketHandler : public WebSocketHandler
{
public:
    void onConnect(const TcpConnectionPtr &conn) override;
    void onMessage(const TcpConnectionPtr &conn, const std::string &message) override;
    void onClose(const TcpConnectionPtr &conn) override;
};

/**
 * @brief [DEMO] 路由参数示例处理器
 */
void demoRouteParamsHandler(const HttpRequest &req, HttpResponse *resp);

/**
 * @brief [DEMO] 受保护的API示例处理器
 */
void demoProtectedHandler(const HttpRequest &req, HttpResponse *resp);
