#pragma once

#include "base/noncopyable.h"

class InetAddress;

/**
 * @brief Socket封装类,封装socket文件描述符
 *
 * Socket类封装了socket文件描述符,提供了便捷的socket操作接口。
 * 主要功能：
 * - 封装socket文件描述符的生命周期管理
 * - 提供常用的socket系统调用封装
 * - 设置常用的socket选项
 * - 自动管理文件描述符的关闭
 *
 * 设计特点：
 * - RAII设计,自动管理文件描述符资源
 * - 不可拷贝,确保文件描述符的唯一性
 * - 提供类型安全的socket操作接口
 * - 简化了socket编程的复杂性
 *
 * 使用场景：
 * - 在Acceptor中封装监听socket
 * - 在TcpConnection中封装连接socket
 * - 需要设置socket选项时
 */
class Socket : noncopyable
{
public:
    /**
     * @brief 构造函数
     * @param sockfd 已创建的socket文件描述符
     *
     * 封装一个已存在的socket文件描述符,不创建新的socket
     */
    explicit Socket(int sockfd) : sockfd_(sockfd) {}

    /**
     * @brief 析构函数
     *
     * 自动关闭socket文件描述符,防止文件描述符泄漏
     */
    ~Socket();

    /**
     * @brief 获取封装的socket文件描述符
     * @return socket文件描述符
     */
    int fd() const { return sockfd_; }

    /**
     * @brief 绑定socket到指定地址
     * @param localaddr 要绑定的本地地址
     *
     * 封装bind系统调用,将socket绑定到指定的IP地址和端口
     */
    void bindAddress(const InetAddress &localaddr);

    /**
     * @brief 开始监听连接请求
     *
     * 封装listen系统调用,使socket进入监听状态
     */
    void listen();

    /**
     * @brief 接受新连接
     * @param peeraddr 输出参数,对端地址
     * @return 新连接的socket文件描述符,失败返回-1
     *
     * 封装accept系统调用,接受新的连接请求
     */
    int accept(InetAddress *peeraddr);

    /**
     * @brief 关闭写端
     *
     * 封装shutdown系统调用,关闭socket的写端,但保持读端开放
     */
    void shutdownWrite();

    /**
     * @brief 设置TCP_NODELAY选项
     * @param on 是否启用Nagle算法,true表示禁用,false表示启用
     *
     * 禁用Nagle算法可以减少延迟,但可能增加网络负载
     */
    void setTcpNoDelay(bool on);

    /**
     * @brief 设置SO_REUSEADDR选项
     * @param on 是否启用地址复用
     *
     * 允许在同一端口上重新绑定socket,即使端口仍处于TIME_WAIT状态
     */
    void setReuseAddr(bool on);

    /**
     * @brief 设置SO_REUSEPORT选项
     * @param on 是否启用端口复用
     *
     * 允许多个socket绑定到相同的IP地址和端口,用于负载均衡
     */
    void setReusePort(bool on);

    /**
     * @brief 设置SO_KEEPALIVE选项
     * @param on 是否启用TCP保活机制
     *
     * 启用TCP保活机制,可以检测到死连接
     */
    void setKeepAlive(bool on);

private:
    const int sockfd_; // socket文件描述符,const确保不被修改
};