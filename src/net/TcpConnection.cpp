#include "TcpConnection.h"

#include <errno.h>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>

#include "Channel.h"
#include "EventLoop.h"
#include "log/Log.h"
#include "Socket.h"
#include "net/TimerId.h"
#include "net/NetworkConfig.h"
#include <openssl/err.h>
#include "ssl/SSLContext.h"

/**
 * @brief 检查事件循环指针是否为空
 * @param loop 事件循环指针
 * @return 如果非空返回原指针，否则终止程序
 * @note 强制要求传入的 EventLoop* loop (baseLoop) 不能为空
 */
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        DLOG_FATAL << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " TcpConnection Loop is null!";
    }
    return loop;
}

/**
 * @brief 构造函数
 * @param loop 事件循环指针
 * @param nameArg 连接名称
 * @param sockfd 套接字文件描述符
 * @param localAddr 本地地址
 * @param peerAddr 对端地址
 * @param config 网络配置
 * @param sslContext SSL上下文
 * @note 初始化TCP连接，设置通道回调，配置SSL（如果提供）
 */
TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                             const InetAddress &localAddr, const InetAddress &peerAddr,
                             std::shared_ptr<NetworkConfig> config, SSLContext *sslContext)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      networkConfig_(config),
      highWaterMark_(64 * 1024 * 1024), // 64M
      isET_(config && config->isET()),
      ssl_(nullptr, &SSL_free)
{
    // 设置 Channel 回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    DLOG_INFO << "TcpConnection::ctor[" << name_ << "] at fd=" << sockfd;
    socket_->setKeepAlive(true);

    if (sslContext)
    {
        SSL *ssl = SSL_new(sslContext->get());
        if (!ssl)
        {
            DLOG_FATAL << "SSL_new error";
            ERR_print_errors_fp(stderr);
            abort();
        }
        ssl_.reset(ssl);
        SSL_set_fd(ssl_.get(), sockfd);
        // 设置为服务器模式
        SSL_set_accept_state(ssl_.get());
    }
}

/**
 * @brief 析构函数
 * @note 记录连接销毁日志
 */
TcpConnection::~TcpConnection()
{
    DLOG_INFO << "TcpConnection::dtor[" << name_ << "] at fd=" << channel_->fd() << " state=" << state_;
}

bool TcpConnection::isBlockedError(int err) const
{
    if (ssl_)
    {
        return err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE;
    }
    return err == EWOULDBLOCK || err == EAGAIN;
}

ssize_t TcpConnection::writeSocket(const void *data, size_t len, int *saveErrno)
{
    if (ssl_)
    {
        return sslWrite(data, len, saveErrno);
    }

    ssize_t n = ::write(channel_->fd(), data, len);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}

bool TcpConnection::flushOutputBuffer()
{
    const size_t BACKPRESSURE_RESUME_THRESHOLD = 512 * 1024; // 512KB

    while (outputBuffer_.readableBytes() > 0)
    {
        int saveErrno = 0;
        ssize_t n = writeSocket(outputBuffer_.peek(), outputBuffer_.readableBytes(), &saveErrno);
        if (n > 0)
        {
            // 移除已成功发送的数据
            outputBuffer_.retrieve(n);

            // 当 outputBuffer 降到阈值以下时恢复文件分块读取，提升吞吐
            if (fileReadContinuation_ && outputBuffer_.readableBytes() < BACKPRESSURE_RESUME_THRESHOLD)
            {
                auto continuation = std::move(fileReadContinuation_);
                fileReadContinuation_ = nullptr;
                loop_->queueInLoop(continuation);
            }

            if (!isET_)
            {
                break;
            }
        }
        else
        {
            if (isBlockedError(saveErrno))
            {
                return false;
            }

            DLOG_ERROR << "TcpConnection::flushOutputBuffer error";
            return false;
        }
    }

    return outputBuffer_.readableBytes() == 0;
}

bool TcpConnection::flushPendingWrite()
{
    if (!flushOutputBuffer())
    {
        channel_->enableWriting();
        return false;
    }

    if (fileSendState_)
    {
        continueSendFile();
        return !fileSendState_ && outputBuffer_.readableBytes() == 0;
    }

    channel_->disableWriting();
    if (writeCompleteCallback_)
    {
        // 防御性编程,确保在下轮事件循环执行回调
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
    }
    if (state_ == kDisconnecting) // 如果正在断开连接,则关闭连接
    {
        shutdownInLoop();
    }
    return true;
}

/**
 * @brief 发送数据
 * @param buf 要发送的数据
 * @note 如果当前线程是事件循环线程则直接发送，否则加入队列
 */
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            // 捕获 shared_ptr 避免对象析构后回调执行导致悬空
            auto self = shared_from_this();
            loop_->runInLoop([self, buf]()
                             { self->sendInLoop(buf.data(), buf.size()); });
        }
    }
}

/**
 * @brief 在事件循环中发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @note 处理发送缓冲区满的情况，支持部分发送
 */
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;      // 记录本次发送的字节数
    size_t remaining = len;  // 记录剩余未发送的字节数,初始为总长度
    bool faultError = false; // 记录是否发生错误

    if (state_ == kDisconnected)
    {
        DLOG_ERROR << "disconnected, give up writing";
        return;
    }

    // 如果正在发送文件，拒绝新的写入，防止响应交织
    if (fileSendState_)
    {
        DLOG_ERROR << "Attempted to send " << len << " bytes while file sending in progress on connection "
                   << name_ << ", force closing to prevent response interleaving";
        forceClose();
        return;
    }

    // 当前Channel关注写事件,且发送缓冲区为空,则尝试将数据写入Socket
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        while (remaining > 0)
        {
            int err = 0;
            nwrote = writeSocket(static_cast<const char *>(data) + (len - remaining), remaining, &err);
            if (nwrote > 0) // 成功写入nwrote字节
            {
                // 更新剩余字节数
                remaining -= nwrote;
                if (!isET_)
                {
                    break;
                }
            }
            else // 写入出错
            {
                // 因为出错,所以并没有写入任何字节
                nwrote = 0;
                if (isBlockedError(err))
                {
                    // 非阻塞IO下的正常情况
                }
                else
                {
                    DLOG_ERROR << "TcpConnection::sendInLoop error";
                    if (err == EPIPE || err == ECONNRESET)
                    {
                        faultError = true;
                    }
                }
                break;
            }
        }
    }

    // 如果全部发送完,就调用写回调
    if (!faultError && remaining == 0 && writeCompleteCallback_)
    {
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
    }

    // 连接未发生错误,但要么未尝试发送,要么只发送了部分数据
    if (!faultError && remaining > 0)
    {
        // 获取当前缓冲区已有数据量
        size_t oldlen = outputBuffer_.readableBytes();

        // 硬限流：防止慢客户端导致内存耗尽
        const size_t MAX_OUTPUT_BUFFER = 64 * 1024 * 1024; // 64MB
        if (oldlen + remaining > MAX_OUTPUT_BUFFER)
        {
            DLOG_ERROR << "outputBuffer would exceed limit (" << MAX_OUTPUT_BUFFER << "), force closing connection";
            forceClose();
            return;
        }

        // 表示本次添加数据后将首次超过高水位线
        if (oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_ &&
            highWaterMarkCallback_)
        {
            // 触发高水位回调
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        // 将未发送的数据追加到 outputBuffer_ 末尾
        size_t sent = len - remaining;
        outputBuffer_.append(static_cast<const char *>(data) + sent, remaining);
        if (!channel_->isWriting()) // 如果Channel未关注写事件
        {
            // 通知Poller关注该connfd的写事件
            channel_->enableWriting();
        }
    }
}

/**
 * @brief 关闭连接
 * @note 设置状态为断开中，在事件循环中执行实际关闭操作
 */
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::forceClose()
{
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

/**
 * @brief 在事件循环中关闭连接
 * @note 如果发送缓冲区为空，则关闭写端
 */
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())
    {
        if (ssl_)
        {
            SSL_shutdown(ssl_.get());
        }
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        handleClose();
    }
}

/**
 * @brief 发送WebSocket数据
 * @param payload 数据载荷
 * @param opcode 操作码
 * @note 将数据编码为WebSocket帧后发送
 */
void TcpConnection::sendWebSocket(const std::string &payload, WebSocketParser::Opcode opcode)
{
    std::string frame = WebSocketParser::encodeFrame(opcode, payload);
    if (loop_->isInLoopThread())
    {
        sendInLoop(frame.c_str(), frame.size());
    }
    else
    {
        // 捕获frame的拷贝
        loop_->runInLoop([self = shared_from_this(), frame]()
                         { self->sendInLoop(frame.c_str(), frame.size()); });
    }
}

/**
 * @brief 连接建立完成
 * @note 设置连接状态，启用读事件，注册空闲超时定时器
 */
void TcpConnection::connectEstablished()
{
    if (ssl_)
    {
        // 如果启用了SSL，则进入握手状态
        setState(KHandshaking);
        channel_->setReadCallback(std::bind(&TcpConnection::handleSSLHandshake, this));
        channel_->setWriteCallback(std::bind(&TcpConnection::handleSSLHandshake, this));
        channel_->enableReading();
        channel_->enableWriting(); // 握手时可能需要读也可能需要写
        handleSSLHandshake();      // 立即尝试握手
    }
    else
    {
        // 普通TCP连接
        setState(kConnected);
        channel_->tie(shared_from_this());
        channel_->enableReading();

        connectionCallback_(shared_from_this());

        // 从成员变量获取超时时间
        int idleTimeout = networkConfig_->getIdleTimeout();
        DLOG_INFO << "[IdleTimeout] 连接 " << name_ << " 设置空闲超时定时器: " << idleTimeout << " 秒";

        auto weakThis = std::weak_ptr<TcpConnection>(shared_from_this());
        idleTimerId_ = loop_->runAfter(static_cast<double>(idleTimeout), [weakThis]
                                       {
        auto strongThis = weakThis.lock();
        if (strongThis) {
            DLOG_INFO << "[IdleTimeout] 连接 " << strongThis->name() << " 超时触发, 关闭连接";
            strongThis->shutdown();
        } });
    }
}
// 连接断开
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }

    // 清理文件发送状态
    if (fileSendState_)
    {
        DLOG_WARN << "Connection destroyed while file sending, closing fd";
        ::close(fileSendState_->fd);
        fileSendState_.reset();
    }

    // 清理分块文件读取状态，断开引用环
    if (fileReadState_)
    {
        DLOG_WARN << "Connection destroyed while file reading, closing fd";
        fileReadState_->closeFile();
        fileReadState_.reset();
    }
    fileReadContinuation_ = nullptr;

    DLOG_INFO << "[IdleTimeout] 连接 " << name_ << " 销毁, 取消空闲定时器";
    loop_->cancel(idleTimerId_);
    channel_->remove();
}

void TcpConnection::sendFile(const std::string &filePath, bool closeAfterSend, const std::function<void()> &completionCallback)
{
    if (state_ == kConnected)
    {
        // 捕获 shared_ptr 避免对象析构后回调执行导致悬空
        auto self = shared_from_this();
        loop_->runInLoop([self, filePath, closeAfterSend, completionCallback]()
                         { self->sendFileInLoop(filePath, closeAfterSend, completionCallback); });
    }
}

void TcpConnection::sendFileInLoop(const std::string &filePath, bool closeAfterSend, const std::function<void()> &completionCallback)
{
    loop_->assertInLoopThread();
    if (state_ == kDisconnected)
    {
        DLOG_ERROR << "disconnected, give up sending file";
        return;
    }

    // SSL 连接不支持 sendfile，回退到读文件+send
    // 使用分块读取 + 背压机制，避免内存耗尽
    if (ssl_)
    {
        DLOG_INFO << "SSL connection, fallback to chunked read+send for file: " << filePath;

        // 打开文件
        int filefd = ::open(filePath.c_str(), O_RDONLY);
        if (filefd < 0)
        {
            DLOG_ERROR << "Failed to open file " << filePath << " for chunked reading, closing connection";
            shutdown();
            return;
        }

        // 创建分块读取状态
        auto readState = std::make_shared<FileReadState>();
        readState->fd = filefd;
        readState->closeAfterSend = closeAfterSend;
        readState->completionCallback = completionCallback;
        fileReadState_ = readState;

        // 禁用读事件，防止在文件发送期间处理新请求导致响应交织
        if (channel_->isReading())
        {
            channel_->disableReading();
            readState->needReenableReading = true;
            DLOG_DEBUG << "Disabled reading during SSL file send to prevent request pipelining";
        }

        // 分块读取并发送
        const size_t CHUNK_SIZE = 64 * 1024; // 64KB
        const size_t MAX_BUFFER_BEFORE_BACKPRESSURE = 1 * 1024 * 1024; // 1MB 背压阈值

        std::weak_ptr<TcpConnection> weakSelf = shared_from_this();
        std::function<void()> sendChunk = [weakSelf, readState, CHUNK_SIZE, MAX_BUFFER_BEFORE_BACKPRESSURE]() {
            // 尝试获取连接对象，如果已析构则直接清理
            auto self = weakSelf.lock();
            if (!self)
            {
                readState->closeFile();
                return;
            }

            // 检查连接状态
            if (self->state_ != kConnected || readState->closed)
            {
                readState->closeFile();
                self->fileReadState_.reset();
                return;
            }

            // 检查背压：如果 outputBuffer 太大，等待写完成后再继续
            if (self->outputBuffer_.readableBytes() > MAX_BUFFER_BEFORE_BACKPRESSURE)
            {
                DLOG_DEBUG << "Backpressure: outputBuffer size " << self->outputBuffer_.readableBytes()
                          << " exceeds threshold, waiting for write complete";
                // 保存 continuation，等待 handleWrite 触发
                if (!self->fileReadContinuation_)
                {
                    self->fileReadContinuation_ = readState->continuation;
                }
                return;
            }

            // 读取一块数据
            char buffer[CHUNK_SIZE];
            ssize_t n = ::read(readState->fd, buffer, CHUNK_SIZE);

            if (n < 0)
            {
                DLOG_ERROR << "Failed to read file chunk, closing connection";

                // 恢复读事件（如果之前被禁用）
                if (readState->needReenableReading && !self->channel_->isReading())
                {
                    self->channel_->enableReading();
                    DLOG_DEBUG << "Re-enabled reading after SSL file read error";
                }

                readState->closeFile();
                self->fileReadState_.reset();
                self->shutdown();
                return;
            }

            if (n == 0)
            {
                // 文件读取完成
                readState->closeFile();
                DLOG_DEBUG << "File chunked read complete";

                // 恢复读事件（如果之前被禁用）
                if (readState->needReenableReading && !self->channel_->isReading())
                {
                    self->channel_->enableReading();
                    DLOG_DEBUG << "Re-enabled reading after SSL file send complete";
                }

                if (readState->closeAfterSend)
                {
                    self->shutdownInLoop();
                }

                // 只有连接仍然正常时才调用 completionCallback
                if (readState->completionCallback && self->state_ == kConnected)
                    readState->completionCallback();

                self->fileReadState_.reset();
                return;
            }

            // 发送这一块数据
            self->sendInLoop(buffer, n);

            // 继续读取下一块（只通过 queueInLoop）
            self->loop_->queueInLoop(readState->continuation);
        };

        readState->continuation = sendChunk;

        // 开始读取第一块
        loop_->queueInLoop(sendChunk);
        return;
    }

    // 打开文件
    int filefd = ::open(filePath.c_str(), O_RDONLY);
    if (filefd < 0)
    {
        DLOG_ERROR << "Failed to open file " << filePath << " for sending, closing connection";
        shutdown();
        return;
    }

    struct stat st;
    if (::fstat(filefd, &st) < 0)
    {
        DLOG_ERROR << "Failed to get stats for file " << filePath << ", closing connection";
        ::close(filefd);
        shutdown();
        return;
    }

    DLOG_INFO << "Sending file " << filePath << " (" << st.st_size << ") bytes using sendfile";

    // 保存文件发送状态
    fileSendState_ = std::make_unique<FileSendState>();
    fileSendState_->fd = filefd;
    fileSendState_->offset = 0;
    fileSendState_->remaining = st.st_size;
    fileSendState_->closeAfterSend = closeAfterSend;
    fileSendState_->completionCallback = completionCallback;

    // 禁用读事件，防止在文件发送期间处理新请求导致响应交织
    if (channel_->isReading())
    {
        channel_->disableReading();
        fileSendState_->needReenableReading = true;
        DLOG_DEBUG << "Disabled reading during file send to prevent request pipelining";
    }

    // 如果 outputBuffer_ 不为空（header 还没发完），只 enableWriting，等待 handleWrite 先把 header 发完
    if (outputBuffer_.readableBytes() > 0)
    {
        DLOG_DEBUG << "outputBuffer has " << outputBuffer_.readableBytes() << " bytes, waiting for header to flush before sendfile";
        channel_->enableWriting();
        return;
    }

    // outputBuffer_ 为空，可以立即开始发送文件
    continueSendFile();
}

void TcpConnection::continueSendFile()
{
    loop_->assertInLoopThread();
    if (!fileSendState_ || state_ == kDisconnected)
    {
        return;
    }

    while (fileSendState_ && fileSendState_->remaining > 0)
    {
        ssize_t nwrote = ::sendfile(channel_->fd(), fileSendState_->fd, &fileSendState_->offset, fileSendState_->remaining);

        if (nwrote < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 需要等待写就绪
                channel_->enableWriting();
                DLOG_DEBUG << "sendfile EAGAIN, wait for writable";
                return;
            }
            else
            {
                DLOG_ERROR << "sendfile error: " << strerror(errno) << ", closing connection";

                // 恢复读事件（如果之前被禁用）
                if (fileSendState_->needReenableReading && !channel_->isReading())
                {
                    channel_->enableReading();
                    DLOG_DEBUG << "Re-enabled reading after sendfile error";
                }

                ::close(fileSendState_->fd);
                fileSendState_.reset();
                shutdown();
                return;
            }
        }

        // 处理 sendfile 返回 0 的情况（文件可能被截断或其他异常）
        if (nwrote == 0 && fileSendState_->remaining > 0)
        {
            DLOG_ERROR << "sendfile returned 0 with remaining " << fileSendState_->remaining << " bytes, closing connection";

            // 恢复读事件（如果之前被禁用）
            if (fileSendState_->needReenableReading && !channel_->isReading())
            {
                channel_->enableReading();
                DLOG_DEBUG << "Re-enabled reading after sendfile returned 0";
            }

            ::close(fileSendState_->fd);
            fileSendState_.reset();
            channel_->disableWriting();
            shutdown();
            return;
        }

        fileSendState_->remaining -= nwrote;
        DLOG_DEBUG << "sendfile wrote " << nwrote << " bytes, remaining " << fileSendState_->remaining;

        if (fileSendState_->remaining == 0)
        {
            // 文件发送完成
            DLOG_INFO << "sendfile completed";
            ::close(fileSendState_->fd);
            bool closeAfterSend = fileSendState_->closeAfterSend;
            bool needReenableReading = fileSendState_->needReenableReading;
            auto callback = std::move(fileSendState_->completionCallback);
            fileSendState_.reset();

            // 恢复读事件（如果之前被禁用）
            if (needReenableReading && !channel_->isReading())
            {
                channel_->enableReading();
                DLOG_DEBUG << "Re-enabled reading after file send complete";
            }

            // 只有在 outputBuffer_ 为空时才 disableWriting
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
            }

            if (closeAfterSend)
            {
                shutdownInLoop();
            }

            // 调用完成回调
            if (callback)
            {
                callback();
            }
            return;
        }

        if (!isET_)
        {
            break;
        }
    }

    // 还有数据未发送，等待写就绪
    channel_->enableWriting();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t totalRead = 0;
    bool peerClosed = false;
    bool hasError = false;

    // 根据是否启用SSL选择不同的读取方式
    if (ssl_)
    {
        totalRead = sslRead(&saveErrno);
        if (totalRead == 0)
        {
            peerClosed = true;
        }
        else if (totalRead < 0)
        {
            hasError = !isBlockedError(saveErrno);
        }
    }
    else if (isET_)
    {
        while (true)
        {
            int readErrno = 0;
            ssize_t n = inputBuffer_.readFd(channel_->fd(), &readErrno);
            if (n > 0)
            {
                totalRead += n;
            }
            else if (n == 0)
            {
                peerClosed = true;
                break;
            }
            else if (readErrno == EWOULDBLOCK || readErrno == EAGAIN)
            {
                saveErrno = readErrno;
                break;
            }
            else
            {
                saveErrno = readErrno;
                hasError = true;
                break;
            }
        }
    }
    else
    {
        totalRead = inputBuffer_.readFd(channel_->fd(), &saveErrno);
        if (totalRead == 0)
        {
            peerClosed = true;
        }
        else if (totalRead < 0)
        {
            hasError = !isBlockedError(saveErrno);
        }
    }

    if (totalRead > 0)
    {
        // 收到消息,重置空闲定时器
        loop_->cancel(idleTimerId_);

        // 从成员变量获取超时时间
        int idleTimeout = networkConfig_->getIdleTimeout();
        DLOG_INFO << "[IdleTimeout] 连接 " << name_ << " 收到消息, 重置空闲超时定时器: " << idleTimeout << " 秒";

        auto weakThis = std::weak_ptr<TcpConnection>(shared_from_this());
        idleTimerId_ = loop_->runAfter(static_cast<double>(idleTimeout), [weakThis]
                                       {
            auto strongThis = weakThis.lock();
            if (strongThis) {
                 DLOG_INFO << "[IdleTimeout] 连接 " << strongThis->name() << " 超时触发, 关闭连接";
                 strongThis->shutdown();
            } });
        // 这是网络库使用者最关心的回调之一(通常对应 onMessage)。
        try
        {
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        catch (const std::exception &e)
        {
            DLOG_ERROR << "Exception in messageCallback for connection " << name_ << ": " << e.what();
            handleError();
            shutdown(); // 关闭有问题的连接
        }
    }

    if (peerClosed) // 对端关闭连接
    {
        handleClose();
    }
    else if (hasError) // 发送错误
    {
        DLOG_ERROR << "TcpConnection::handleRead error";
        handleError();
    }
}

void TcpConnection::handleWrite()
// 当 Poller 检测到 connfd 变为可写时 并且 Channel 当前正关注写事件
// (通常是因为上次 send操作未能一次性将 outputBuffer_ 中的数据全部发送出去)
{
    // 检查写状态
    if (channel_->isWriting())
    {
        if (!flushPendingWrite())
        {
            return;
        }
        return;
    }
    else // Channel不在写状态,却调用了handleWrite,异常
    {
        DLOG_ERROR << "TcpConnection fd=" << channel_->fd() << " is down, no more writing";
    }
}

void TcpConnection::handleClose()
{
    DLOG_INFO << "TcpConnection::handleClose fd=" << channel_->fd() << " state=" << state_;
    // 将连接状态更新为已断开
    setState(kDisconnected);
    // 移除 Channel
    channel_->disableAll();
    channel_->remove();
    // 执行连接断开回调函数
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    DLOG_ERROR << "TcpConnection::handleError name:" << name_ << " - SO_ERROR:" << err;
}

void TcpConnection::handleSSLHandshake()
{
    if (state_ != KHandshaking)
        return;

    int ret = SSL_do_handshake(ssl_.get());
    if (ret == 1)
    {
        // 握手成功
        setState(kConnected);
        DLOG_INFO << "[SSL] Handshake success for " << name();
        // 恢复正常的回调
        channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
        channel_->disableWriting();              // 握手后通常先等待读
        connectionCallback_(shared_from_this()); // 触发连接建立回调

        // 为连接成功设置空闲超时定时器
        int idleTimeout = networkConfig_->getIdleTimeout();
        DLOG_INFO << "[IdleTimeout] 连接 " << name_ << " 设置空闲超时定时器: " << idleTimeout << " 秒";
        auto weakThis = std::weak_ptr<TcpConnection>(shared_from_this());
        idleTimerId_ = loop_->runAfter(static_cast<double>(idleTimeout), [weakThis]
                                       {
            auto strongThis = weakThis.lock();
            if (strongThis) {
                DLOG_INFO << "[IdleTimeout] 连接 " << strongThis->name() << " 超时触发, 关闭连接";
                strongThis->shutdown();
            } });
    }
    else
    {
        int err = SSL_get_error(ssl_.get(), ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            channel_->enableReading();
            channel_->disableWriting();
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            channel_->enableWriting();
            channel_->disableReading();
        }
        else
        {
            // 从OpenSSL的错误队列中获取更详细的错误信息
            unsigned long errCode = ERR_get_error();
            char errBuf[256];
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));

            // 判断是否是客户端不信任我们的证书导致的“正常”失败
            if (strstr(errBuf, "certificate unknown") != nullptr)
            {
                // 将日志级别降为INFO，并使用中文提示
                DLOG_INFO << "[SSL] 客户端浏览器拒绝了服务器的自签名证书，这是一个正常的开发期行为。连接: " << name();
            }
            else
            {
                // 对于其他真正的错误，我们仍然作为ERROR记录
                DLOG_ERROR << "[SSL] 握手失败, 连接: " << name()
                           << ", OpenSSL错误码: " << err
                           << ", 详细信息: " << errBuf;
            }
            handleClose();
        }
    }
}

ssize_t TcpConnection::sslRead(int *saveErrno)
{
    ssize_t n = 0;
    while (true)
    {
        char buf[65536];
        int ret = SSL_read(ssl_.get(), buf, sizeof(buf));
        if (ret > 0)
        {
            inputBuffer_.append(buf, ret);
            n += ret;
        }
        else
        {
            *saveErrno = SSL_get_error(ssl_.get(), ret);
            if (*saveErrno != SSL_ERROR_WANT_READ && *saveErrno != SSL_ERROR_WANT_WRITE)
            {
                // 真正发生错误或对方关闭
                return (ret == 0) ? 0 : -1;
            }
            // 需要更多数据，中断循环
            break;
        }
    }
    return n;
}

ssize_t TcpConnection::sslWrite(const void *data, size_t len, int *saveErrno)
{
    int ret = SSL_write(ssl_.get(), data, len);
    if (ret <= 0)
    {
        *saveErrno = SSL_get_error(ssl_.get(), ret);
    }
    return ret;
}
