#include "Socket.h"

#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "InetAddress.h"
#include "log/Log.h"

/**
 * @brief 析构函数
 * @details 自动关闭socket文件描述符，防止文件描述符泄漏
 */
Socket::~Socket() { close(sockfd_); }

/**
 * @brief 绑定socket到指定地址
 * @param localaddr 要绑定的本地地址
 * @details 封装bind系统调用，将socket绑定到指定的IP地址和端口
 */
void Socket::bindAddress(const InetAddress &localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        DLOG_FATAL << "bind sockfd:" << sockfd_ << " fail";
    }
}

/**
 * @brief 开始监听连接请求
 * @details 封装listen系统调用，使socket进入监听状态，设置最大连接队列长度为1024
 */
void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        DLOG_FATAL << "listen sockfd:" << sockfd_ << " fail";
    }
}

/**
 * @brief 接受新连接
 * @param peeraddr 输出参数，对端地址
 * @return 新连接的socket文件描述符，失败返回-1
 * @details 封装accept4系统调用，接受新的连接请求，并设置非阻塞和close-on-exec标志
 */
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr); // 清空地址结构体
    // 接收新的连接请求，同时设置文件描述符为非阻塞和执行时关闭
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
    {
        // 如果成功接收到新的连接，将对方的地址信息存储到peeraddr中
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

/**
 * @brief 关闭写端
 * @details 封装shutdown系统调用，关闭socket的写端，但保持读端开放
 */
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        DLOG_ERROR << "shutdownWrite error";
    }
}

/**
 * @brief 设置TCP_NODELAY选项
 * @param on 是否启用Nagle算法，true表示禁用，false表示启用
 * @details 禁用Nagle算法可以减少延迟，但可能增加网络负载
 */
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0; // 将布尔值转换为整数选项值
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

/**
 * @brief 设置SO_REUSEADDR选项
 * @param on 是否启用地址复用
 * @details 允许在同一端口上重新绑定socket，即使端口仍处于TIME_WAIT状态
 */
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0; // 将布尔值转换为整数选项值
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

/**
 * @brief 设置SO_REUSEPORT选项
 * @param on 是否启用端口复用
 * @details 允许多个socket绑定到相同的IP地址和端口，用于负载均衡
 */
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0; // 将布尔值转换为整数选项值
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

/**
 * @brief 设置SO_KEEPALIVE选项
 * @param on 是否启用TCP保活机制
 * @details 启用TCP保活机制，可以检测到死连接
 */
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0; // 将布尔值转换为整数选项值
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}