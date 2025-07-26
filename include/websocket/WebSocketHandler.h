#pragma once
#include <functional>
#include <memory>
#include <string>

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief WebSocket连接生命周期回调接口
 */
class WebSocketHandler
{
public:
    using Ptr = std::shared_ptr<WebSocketHandler>;

    virtual ~WebSocketHandler() = default;
    virtual void onConnect(const TcpConnectionPtr &conn) = 0;
    virtual void onMessage(const TcpConnectionPtr &conn, const std::string &message) = 0;
    virtual void onClose(const TcpConnectionPtr &conn) = 0;
};