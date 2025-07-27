#pragma once
#include <functional>
#include <memory>
#include <string>

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief WebSocket连接生命周期回调接口
 * @details 定义了WebSocket连接的各种事件回调函数，包括连接建立、消息接收、连接关闭等
 */
class WebSocketHandler
{
public:
    using Ptr = std::shared_ptr<WebSocketHandler>;

    /**
     * @brief 虚析构函数
     * @details 确保派生类能够正确析构
     */
    virtual ~WebSocketHandler() = default;

    /**
     * @brief 连接建立回调
     * @param conn TCP连接指针
     * @details 当WebSocket连接建立时调用此函数
     */
    virtual void onConnect(const TcpConnectionPtr &conn) = 0;

    /**
     * @brief 消息接收回调
     * @param conn TCP连接指针
     * @param message 接收到的消息内容
     * @details 当接收到WebSocket消息时调用此函数
     */
    virtual void onMessage(const TcpConnectionPtr &conn, const std::string &message) = 0;

    /**
     * @brief 连接关闭回调
     * @param conn TCP连接指针
     * @details 当WebSocket连接关闭时调用此函数
     */
    virtual void onClose(const TcpConnectionPtr &conn) = 0;
};