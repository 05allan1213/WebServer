#pragma once

#include "HttpParser.h"
#include "websocket/WebSocketParser.h"
#include "websocket/WebSocketHandler.h"
#include <memory>

/**
 * @brief 统一的套接字上下文
 * 封装了一个连接在HTTP和WebSocket两个阶段所需的所有解析器和处理器。
 */
struct SocketContext
{
    enum State
    {
        HTTP,
        WEBSOCKET
    };

    State state;
    HttpParser httpParser;
    WebSocketParser wsParser;
    WebSocketHandler::Ptr wsHandler;

    SocketContext() : state(HTTP) {}
};