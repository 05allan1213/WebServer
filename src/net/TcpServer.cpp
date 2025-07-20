#include "TcpServer.h"

#include <functional>
#include <strings.h>
#include <thread>
#include "log/Log.h"
#include "TcpConnection.h"
#include "log/LogConfig.h"
#include "net/NetworkConfig.h"

// 强制要求传入的 EventLoop* loop (baseLoop) 不能为空
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        DLOG_FATAL << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " mainLoop is null!";
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      // 将loop(baseLoop)传递给Acceptor，明确Acceptor在baseLoop中执行
      // 将监听地址(listenAddr)传递给 Acceptor，用于后续的 socket, bind, listen 操作
      // 根据 option 决定是否设置 SO_REUSEPORT 选项
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      // 此处只创建线程池对象，还未启动任何IO线程(subLoop)
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    DLOG_INFO << "TcpServer 构造函数开始 - 名称: " << name_ << ", 监听地址: " << ipPort_;

    // 自动加载配置文件（只加载一次）
    DLOG_INFO << "加载网络配置文件: configs/config.yml";
    NetworkConfig::getInstance().load("configs/config.yml");

    // 确保日志目录存在
    DLOG_INFO << "创建日志目录: logs";
    system("mkdir -p logs");

    // 等待异步日志系统启动完成
    DLOG_INFO << "等待异步日志系统启动...";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 初始化日志系统（使用无参版本，自动从配置读取参数）
    DLOG_INFO << "初始化日志系统...";
    initLogSystem();
    DLOG_INFO << "日志系统初始化完成";

    // 应用线程池配置
    DLOG_INFO << "开始应用线程池配置...";
    const auto &netConfig = NetworkConfig::getInstance();

    DLOG_INFO << "网络配置详情:";
    DLOG_INFO << "  - IP地址: " << netConfig.getIp();
    DLOG_INFO << "  - 端口: " << netConfig.getPort();
    DLOG_INFO << "  - 线程数: " << netConfig.getThreadNum();
    DLOG_INFO << "  - 队列大小: " << netConfig.getThreadPoolQueueSize();
    DLOG_INFO << "  - 保活时间: " << netConfig.getThreadPoolKeepAliveTime() << "秒";
    DLOG_INFO << "  - 最大空闲线程: " << netConfig.getThreadPoolMaxIdleThreads();
    DLOG_INFO << "  - 最小空闲线程: " << netConfig.getThreadPoolMinIdleThreads();

    DLOG_INFO << "应用线程池配置到EventLoopThreadPool...";
    threadPool_->setThreadNum(netConfig.getThreadNum());
    threadPool_->setQueueSize(netConfig.getThreadPoolQueueSize());
    threadPool_->setKeepAliveTime(netConfig.getThreadPoolKeepAliveTime());
    threadPool_->setMaxIdleThreads(netConfig.getThreadPoolMaxIdleThreads());
    threadPool_->setMinIdleThreads(netConfig.getThreadPoolMinIdleThreads());
    DLOG_INFO << "线程池配置应用完成";

    DLOG_INFO << "设置Acceptor新连接回调...";
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));

    DLOG_INFO << "TcpServer 构造函数完成 - 名称: " << name_;
}

TcpServer::~TcpServer()
{
    DLOG_INFO << "TcpServer 析构函数开始 - 名称: " << name_;

    // 遍历并关闭所有连接
    DLOG_INFO << "关闭所有连接，连接数: " << connections_.size();
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 获取连接的shared_ptr
        DLOG_INFO << "关闭连接: " << conn->name();
        item.second.reset(); // 置空shared_ptr，断开TcpServer对TcpConnection对象的强引用
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }

    DLOG_INFO << "TcpServer 析构函数完成 - 名称: " << name_;
}

void TcpServer::setThreadNum(int numThreads)
{
    DLOG_INFO << "TcpServer 设置线程数: " << numThreads;
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    DLOG_INFO << "TcpServer 启动开始 - 名称: " << name_ << ", 当前启动状态: " << started_;

    // 防止重复启动
    if (started_++ == 0)
    {
        DLOG_INFO << "首次启动，开始初始化...";

        DLOG_INFO << "启动线程池...";
        threadPool_->start(threadInitCallback_);
        DLOG_INFO << "线程池启动完成";

        DLOG_INFO << "在主事件循环中启动监听...";
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
        DLOG_INFO << "监听启动完成";

        DLOG_INFO << "TcpServer 启动完成 - 名称: " << name_ << ", 监听地址: " << ipPort_;
    }
    else
    {
        DLOG_WARN << "TcpServer 已经启动，忽略重复启动请求";
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    DLOG_INFO << "收到新连接请求 - sockfd: " << sockfd << ", 客户端地址: " << peerAddr.toIpPort();

    // 轮询获取一个subLoop，以管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    DLOG_INFO << "为新连接分配EventLoop: " << ioLoop;

    // 生成连接名称
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    DLOG_INFO << "生成连接名称: " << connName;

    DLOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection [" << connName << "] from " << peerAddr.toIpPort();

    // 获取本地地址
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        DLOG_ERROR << "sockets::getLocalAddr";
    }
    InetAddress localAddr(local);
    DLOG_INFO << "本地地址: " << localAddr.toIpPort();

    // 创建TcpConnection对象
    DLOG_INFO << "创建TcpConnection对象...";
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    DLOG_INFO << "TcpConnection对象创建成功: " << conn.get();

    // 存储新连接
    connections_[connName] = conn;
    DLOG_INFO << "连接已添加到连接池，当前连接数: " << connections_.size();

    // 设置TcpConnection回调
    DLOG_INFO << "设置TcpConnection回调函数...";
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置内部关闭回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    DLOG_INFO << "回调函数设置完成";

    // 启动TCPConnection
    DLOG_INFO << "在EventLoop中启动TcpConnection...";
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
    DLOG_INFO << "TcpConnection启动完成";
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    DLOG_INFO << "请求移除连接: " << conn->name();
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
// 此方法总是在TCPServer所属的 mainLoop 线程中执行
{
    DLOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    // 从map中删除
    connections_.erase(conn->name());
    DLOG_INFO << "连接已从连接池移除，当前连接数: " << connections_.size();

    // 获取连接所属的subLoop
    EventLoop *ioLoop = conn->getLoop();
    DLOG_INFO << "在EventLoop中销毁连接: " << ioLoop;

    // 删除连接
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    DLOG_INFO << "连接销毁任务已加入队列";
}