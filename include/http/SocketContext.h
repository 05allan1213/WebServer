#pragma once

#include "HttpParser.h"
#include "websocket/WebSocketParser.h"
#include "websocket/WebSocketHandler.h"
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>

/**
 * @brief 统一的套接字上下文
 *
 * 封装了一个连接在HTTP和WebSocket两个阶段所需的所有解析器和处理器。
 * 支持协议升级和状态管理。
 * 支持 per-connection 串行调度，避免同一连接的响应乱序。
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

    using PendingTask = std::function<void()>;

    State state;                     // 当前连接状态
    HttpParser httpParser;           // HTTP解析器
    WebSocketParser wsParser;        // WebSocket解析器
    WebSocketHandler::Ptr wsHandler; // WebSocket处理器

    // Per-connection 串行调度支持
    std::atomic<bool> processingRequest; // 是否有请求正在处理中
    std::queue<PendingTask> pendingTasks; // 待处理的请求队列
    std::mutex taskMutex;                 // 保护待处理队列的互斥锁

    /**
     * @brief 构造函数，默认初始化为HTTP状态
     */
    SocketContext() : state(HTTP), processingRequest(false) {}
};