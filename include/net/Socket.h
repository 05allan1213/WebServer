#pragma once

#include "base/noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    // 构造函数，接收一个已创建的 socket fd
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    // 析构函数，关闭 sockfd
    ~Socket();

    // 获取封装的 fd
    int fd() const { return sockfd_; }
    // 封装 bind 系统调用
    void bindAddress(const InetAddress &localaddr);
    // 封装 listen 系统调用
    void listen();
    // 封装 accept 系统调用
    int accept(InetAddress *peeraddr);
    // 封装 shutdown 系统调用,关闭写端
    void shutdownWrite();

    // 设置 TCP_NODELAY 选项
    void setTcpNoDelay(bool on);
    // 设置 SO_REUSEADDR 选项
    void setReuseAddr(bool on);
    // 设置 SO_REUSEPORT 选项
    void setReusePort(bool on);
    // 设置 SO_KEEPALIVE 选项
    void setKeepAlive(bool on);

private:
    const int sockfd_; // socket fd
};