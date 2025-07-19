#pragma once

#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;  // 定义TcpConnection类型的智能指针
using ConnectionCallback =
    std::function<void(const TcpConnectionPtr&)>;  // 当有新连接建立或连接断开时，调用相应的回调
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;  // 当连接断开时，调用相应的回调
using WriteCompleteCallback =
    std::function<void(const TcpConnectionPtr&)>;  // 当底层发送缓冲区的数据发送完时，调用相应的回调
using MessageCallback = std::function<void(
    const TcpConnectionPtr&, Buffer*, Timestamp)>;  // 当已连接的客户端有数据可读时，调用相应的回调
using HighWaterMarkCallback = std::function<void(
    const TcpConnectionPtr&, size_t)>;  // 当发送缓冲区超过设定值时，调用相应的回调