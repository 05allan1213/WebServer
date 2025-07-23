#pragma once

#include <functional>
#include <memory>
#include <vector>

class Buffer;
class TcpConnection;
class Timestamp;
class HttpRequest;
class HttpResponse;

/** @brief TcpConnection的智能指针类型 */
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief 连接建立/断开回调函数类型
 *
 * 当有新连接建立或连接断开时调用,参数为连接对象的智能指针。
 * 用户可以通过此回调函数处理连接的建立和断开事件。
 */
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 连接关闭回调函数类型
 *
 * 当连接断开时调用,参数为连接对象的智能指针。
 * 这个回调通常由TcpConnection内部调用,用于通知TcpServer连接已关闭。
 */
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 写完成回调函数类型
 *
 * 当底层发送缓冲区的数据发送完毕时调用,参数为连接对象的智能指针。
 * 用户可以通过此回调函数实现流量控制或确认机制。
 */
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 消息到达回调函数类型
 *
 * 当已连接的客户端有数据可读时调用。
 * @param conn 连接对象的智能指针
 * @param buffer 接收缓冲区,包含接收到的数据
 * @param receiveTime 数据接收的时间戳
 *
 * 这是最常用的回调函数,用户在这里处理接收到的数据。
 */
using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;

/**
 * @brief 高水位回调函数类型
 *
 * 当发送缓冲区超过设定值时调用,用于流量控制。
 * @param conn 连接对象的智能指针
 * @param len 当前发送缓冲区的数据长度
 *
 * 用户可以通过此回调函数实现背压机制,防止发送缓冲区无限增长。
 */
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

/**
 * @brief 'next' 回调函数类型
 * @details 中间件通过调用此函数将控制权传递给处理链中的下一个中间件。
 */
using Next = std::function<void()>;

/**
 * @brief HTTP 业务处理器函数类型
 * @details 这是中间件处理链的终点，负责核心的业务逻辑。
 */
using HttpHandler = std::function<void(const HttpRequest &, HttpResponse *)>;

/**
 * @brief 中间件函数类型
 * @details 一个中间件接收请求、响应和 next 回调作为参数。
 */
using Middleware = std::function<void(const HttpRequest &, HttpResponse *, Next)>;

/**
 * @brief 中间件链类型
 */
using MiddlewareChain = std::vector<Middleware>;