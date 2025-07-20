#include "Acceptor.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "InetAddress.h"
#include "log/Log.h"

// 创建非阻塞的socket文件描述符
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        DLOG_FATAL << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " listen socket create err:" << errno;
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      // 创建socket文件描述符
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    // 设置socket选项
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    // 绑定地址
    acceptSocket_.bindAddress(listenAddr);
    // 设置新连接回调函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    // 将 Channel 的所有事件置为无效
    acceptChannel_.disableAll();
    // 将 Channel 从 Poller 中彻底移除
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    // 监听socket
    acceptSocket_.listen();
    // 监听socket可读事件(新连接)
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_) // 回调有效
        {
            // 调用 newConnectionCallback_(将新连接交给上层处理)
            newConnectionCallback_(connfd, peerAddr);
        }
        else // 回调无效
        {
            // 理论上不应发生,表示 TcpServer 未正确设置回调
            ::close(connfd);
        }
    }
    else
    {
        DLOG_ERROR << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " accept err:" << errno;
        if (errno == EMFILE)
        {
            DLOG_ERROR << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " sockfd reached limit!";
        }
    }
}