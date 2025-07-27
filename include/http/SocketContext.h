#pragma once

#include "HttpParser.h"
#include "websocket/WebSocketParser.h"
#include "websocket/WebSocketHandler.h"
#include <memory>

/**
 * @brief 统一的套接字上下文
 *
 * 封装了一个连接在HTTP和WebSocket两个阶段所需的所有解析器和处理器。
 * 支持协议升级和状态管理。
 */
struct SocketContext
{
    /**
     * @brief 连接状态枚举
     */
    enum State
    {
        HTTP,     // HTTP协议状态
        WEBSOCKET // WebSocket协议状态
    };

    State state;                     // 当前连接状态
    HttpParser httpParser;           // HTTP解析器
    WebSocketParser wsParser;        // WebSocket解析器
    WebSocketHandler::Ptr wsHandler; // WebSocket处理器

    /**
     * @brief 构造函数，默认初始化为HTTP状态
     */
    SocketContext() : state(HTTP) {}
};