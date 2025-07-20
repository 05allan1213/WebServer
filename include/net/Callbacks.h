#pragma once

#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;

/**
 * @brief 网络库中使用的各种回调函数类型定义
 *
 * 这个文件定义了网络库中所有用到的回调函数类型，包括：
 * - 连接相关的回调（建立、断开）
 * - 数据传输相关的回调（消息到达、写完成）
 * - 流量控制相关的回调（高水位）
 *
 * 所有回调函数都使用std::function包装，支持函数指针、成员函数、lambda表达式等。
 * 回调函数的参数通常包含TcpConnectionPtr，方便访问连接信息和进行数据操作。
 */

/** @brief TcpConnection的智能指针类型 */
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief 连接建立/断开回调函数类型
 *
 * 当有新连接建立或连接断开时调用，参数为连接对象的智能指针。
 * 用户可以通过此回调函数处理连接的建立和断开事件。
 */
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 连接关闭回调函数类型
 *
 * 当连接断开时调用，参数为连接对象的智能指针。
 * 这个回调通常由TcpConnection内部调用，用于通知TcpServer连接已关闭。
 */
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 写完成回调函数类型
 *
 * 当底层发送缓冲区的数据发送完毕时调用，参数为连接对象的智能指针。
 * 用户可以通过此回调函数实现流量控制或确认机制。
 */
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * @brief 消息到达回调函数类型
 *
 * 当已连接的客户端有数据可读时调用。
 * @param conn 连接对象的智能指针
 * @param buffer 接收缓冲区，包含接收到的数据
 * @param receiveTime 数据接收的时间戳
 *
 * 这是最常用的回调函数，用户在这里处理接收到的数据。
 */
using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;

/**
 * @brief 高水位回调函数类型
 *
 * 当发送缓冲区超过设定值时调用，用于流量控制。
 * @param conn 连接对象的智能指针
 * @param len 当前发送缓冲区的数据长度
 *
 * 用户可以通过此回调函数实现背压机制，防止发送缓冲区无限增长。
 */
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;