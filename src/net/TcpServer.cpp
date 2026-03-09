#include "TcpServer.h"
#include <functional>
#include <strings.h>
#include <thread>
#include "log/Log.h"
#include "TcpConnection.h"
#include "base/ConfigManager.h"
#include "ssl/SSLContext.h"

/**
 * @brief 检查EventLoop指针是否为空
 * @param loop EventLoop指针
 * @return 如果非空返回原指针，否则程序终止
 * @details 强制要求传入的EventLoop指针不能为空，用于调试
 */
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        DLOG_FATAL << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " mainLoop is null!";
    }
    return loop;
}

/**
 * @brief 构造函数
 * @param loop 主EventLoop指针
 * @param listenAddr 监听地址
 * @param nameArg 服务器名称
 * @param config 网络配置
 * @param option 服务器选项
 * @details 初始化TcpServer，创建Acceptor和线程池，设置新连接回调
 */
TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,
                     std::shared_ptr<NetworkConfig> config, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      // 将loop(baseLoop)传递给Acceptor,明确Acceptor在baseLoop中执行
      // 将监听地址(listenAddr)传递给 Acceptor,用于后续的 socket, bind, listen 操作
      // 根据 option 决定是否设置 SO_REUSEPORT 选项
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort, config->isET())),
      // 此处只创建线程池对象,还未启动任何IO线程(subLoop)
      // 读取 epoll_mode 配置并传递给线程池
      threadPool_(new EventLoopThreadPool(loop, name_, config->getEpollMode())),
      networkConfig_(config),
      sslContext_(nullptr),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    DLOG_INFO << "TcpServer 构造函数开始 - 名称: " << name_ << ", 监听地址: " << ipPort_;

    threadPool_->setThreadNum(networkConfig_->getThreadNum());
    threadPool_->setQueueSize(networkConfig_->getThreadPoolQueueSize());
    threadPool_->setKeepAliveTime(networkConfig_->getThreadPoolKeepAliveTime());
    threadPool_->setMaxIdleThreads(networkConfig_->getThreadPoolMaxIdleThreads());
    threadPool_->setMinIdleThreads(networkConfig_->getThreadPoolMinIdleThreads());

    DLOG_INFO << "设置Acceptor新连接回调...";
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));

    DLOG_INFO << "TcpServer 构造函数完成 - 名称: " << name_;
}

/**
 * @brief 析构函数
 * @details 关闭所有连接，清理资源
 */
TcpServer::~TcpServer()
{
    DLOG_INFO << "TcpServer 析构函数开始 - 名称: " << name_;

    // 遍历并关闭所有连接
    DLOG_INFO << "关闭所有连接,连接数: " << connections_.size();
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 获取连接的shared_ptr
        DLOG_INFO << "关闭连接: " << conn->name();
        item.second.reset(); // 置空shared_ptr,断开TcpServer对TcpConnection对象的强引用
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }

    DLOG_INFO << "[SERVER] 已关闭 - 名称: " << name_ << ", 监听地址: " << ipPort_;
    DLOG_INFO << "TcpServer 析构函数完成 - 名称: " << name_;
}

/**
 * @brief 启用SSL/TLS支持
 * @param certPath 证书文件路径
 * @param keyPath 私钥文件路径
 * @details 创建SSLContext对象，为后续的SSL连接做准备
 */
void TcpServer::enableSSL(const std::string &certPath, const std::string &keyPath)
{
    DLOG_INFO << "[TcpServer] 启用 SSL/TLS. 证书路径: " << certPath;
    sslContext_ = std::make_unique<SSLContext>(certPath, keyPath);
}

/**
 * @brief 设置线程池线程数量
 * @param numThreads 线程数量
 * @details 设置IO线程池中的线程数量
 */
void TcpServer::setThreadNum(int numThreads)
{
    DLOG_INFO << "TcpServer 设置线程数: " << numThreads;
    threadPool_->setThreadNum(numThreads);
}

/**
 * @brief 启动服务器
 * @details 启动线程池，开始监听连接请求
 */
void TcpServer::start()
{
    DLOG_INFO << "TcpServer 启动开始 - 名称: " << name_ << ", 当前启动状态: " << started_;

    if (started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
        DLOG_INFO << "[SERVER] 启动完成 - 名称: " << name_ << ", 监听地址: " << ipPort_
                  << ", epoll_mode: " << networkConfig_->getEpollMode(); // <-- 使用成员变量
    }
    else
    {
        DLOG_WARN << "TcpServer 已经启动,忽略重复启动请求";
    }
}

/**
 * @brief 处理新连接
 * @param sockfd 新连接的socket文件描述符
 * @param peerAddr 对端地址
 * @details 创建TcpConnection对象，设置回调函数，在IO线程中建立连接
 */
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        DLOG_ERROR << "sockets::getLocalAddr";
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr, networkConfig_, sslContext_.get()));

    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

/**
 * @brief 移除连接
 * @param conn 要移除的连接指针
 * @details 在主线程中请求移除连接，实际移除操作在IO线程中执行
 */
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    DLOG_INFO << "请求移除连接: " << conn->name();
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

/**
 * @brief 在IO线程中移除连接
 * @param conn 要移除的连接指针
 * @details 从连接池中移除连接，并在IO线程中销毁连接对象
 */
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
// 此方法总是在TCPServer所属的 mainLoop 线程中执行
{
    DLOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    // 从map中删除
    connections_.erase(conn->name());
    DLOG_INFO << "连接已从连接池移除,当前连接数: " << connections_.size();

    // 获取连接所属的subLoop
    EventLoop *ioLoop = conn->getLoop();
    DLOG_INFO << "在EventLoop中销毁连接: " << ioLoop;

    // 删除连接
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    DLOG_INFO << "连接销毁任务已加入队列";
}

/**
 * @brief 更新网络配置
 * @param config 新的网络配置对象
 * @note 仅影响新建连接，已有连接保持旧配置
 */
void TcpServer::updateNetworkConfig(std::shared_ptr<NetworkConfig> config)
{
    loop_->assertInLoopThread();
    networkConfig_ = config;
    DLOG_INFO << "[TcpServer] 网络配置已更新（仅新连接生效）";
}
