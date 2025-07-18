#include <functional>
#include <string>
#include "net/TcpServer.h"
#include "log/Log.h"
#include "base/Config.h"

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name, int threadNum)
        : server_(loop, addr, name), loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1,
                                             std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量 loopthread
        server_.setThreadNum(threadNum);
    }
    void start() { server_.start(); }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            DLOG_INFO << "Connection UP : " << conn->peerAddress().toIpPort();
        }
        else
        {
            DLOG_INFO << "Connection DOWN : " << conn->peerAddress().toIpPort();
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown(); // 写端   EPOLLHUP -> closeCallback_
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    Config::getInstance().load("configs/config.yml");
    const auto &config = Config::getInstance();
    std::string ip = config.getNetworkIp();
    int port = config.getNetworkPort();
    int threadNum = config.getNetworkThreadNum();

    EventLoop loop;
    InetAddress addr(port, ip);
    EchoServer server(&loop, addr, "EchoServer-01", threadNum);
    server.start();
    loop.loop();

    return 0;
}