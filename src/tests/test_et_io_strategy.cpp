#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <cstring>

#include <yaml-cpp/yaml.h>

#define private public
#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"
#undef private

namespace
{
std::shared_ptr<NetworkConfig> makeNetworkConfig(const std::string &epollMode)
{
    YAML::Node node = YAML::Load(R"(
ip: 127.0.0.1
port: 18080
thread_pool:
  thread_num: 1
  queue_size: 16
  keep_alive_time: 60
  max_idle_threads: 1
  min_idle_threads: 1
idle_timeout: 30
)");
    node["epoll_mode"] = epollMode;
    return std::make_shared<NetworkConfig>(node);
}

uint16_t getSocketPort(int fd)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    memset(&addr, 0, sizeof addr);
    EXPECT_EQ(::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len), 0);
    return ntohs(addr.sin_port);
}

int createClient(uint16_t port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");

    EXPECT_EQ(::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr), 0);
    return fd;
}

void closeAll(const std::vector<int> &fds)
{
    for (int fd : fds)
    {
        if (fd >= 0)
        {
            ::close(fd);
        }
    }
}

void waitUntilReadable(int fd)
{
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    ASSERT_EQ(::poll(&pfd, 1, 1000), 1);
    ASSERT_TRUE(pfd.revents & POLLIN);
}

void writeAll(int fd, const std::string &data)
{
    size_t written = 0;
    while (written < data.size())
    {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        ASSERT_GT(n, 0);
        written += static_cast<size_t>(n);
    }
}

std::string readAll(int fd, size_t len)
{
    std::string data(len, '\0');
    size_t readBytes = 0;
    while (readBytes < len)
    {
        ssize_t n = ::read(fd, &data[readBytes], len - readBytes);
        if (n <= 0)
        {
            ADD_FAILURE() << "readAll failed";
            break;
        }
        readBytes += static_cast<size_t>(n);
    }
    return data;
}

void createTcpPair(int *serverFd, int *clientFd, InetAddress *localAddr, InetAddress *peerAddr)
{
    int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listenfd, 0);

    int on = 1;
    ASSERT_EQ(::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on), 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");

    ASSERT_EQ(::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof addr), 0);
    ASSERT_EQ(::listen(listenfd, 16), 0);

    uint16_t port = getSocketPort(listenfd);
    *clientFd = createClient(port);

    sockaddr_in peer;
    socklen_t peerLen = sizeof peer;
    memset(&peer, 0, sizeof peer);
    *serverFd = ::accept4(listenfd, reinterpret_cast<sockaddr *>(&peer), &peerLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    ASSERT_GE(*serverFd, 0);

    sockaddr_in local;
    socklen_t localLen = sizeof local;
    memset(&local, 0, sizeof local);
    ASSERT_EQ(::getsockname(*serverFd, reinterpret_cast<sockaddr *>(&local), &localLen), 0);

    *localAddr = InetAddress(local);
    *peerAddr = InetAddress(peer);
    ::close(listenfd);
}

void createSocketPair(int *serverFd, int *clientFd, InetAddress *localAddr, InetAddress *peerAddr)
{
    int fds[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    int flags = ::fcntl(fds[0], F_GETFL, 0);
    ASSERT_GE(flags, 0);
    ASSERT_EQ(::fcntl(fds[0], F_SETFL, flags | O_NONBLOCK), 0);

    *serverFd = fds[0];
    *clientFd = fds[1];
    *localAddr = InetAddress();
    *peerAddr = InetAddress();
}

TcpConnectionPtr createConnection(EventLoop *loop, const std::string &mode, int serverFd,
                                  const InetAddress &localAddr, const InetAddress &peerAddr)
{
    auto config = makeNetworkConfig(mode);
    TcpConnectionPtr conn(new TcpConnection(loop, "test-conn", serverFd, localAddr, peerAddr, config, nullptr));
    conn->setConnectionCallback([](const TcpConnectionPtr &) {});
    conn->setCloseCallback([](const TcpConnectionPtr &) {});
    conn->setMessageCallback([](const TcpConnectionPtr &, Buffer *, Timestamp) {});
    conn->connectEstablished();
    return conn;
}
} // namespace

TEST(ETIOTest, AcceptorDrainsAllPendingConnectionsInETMode)
{
    EventLoop loop("ET");
    Acceptor acceptor(&loop, InetAddress(0, "127.0.0.1"), false, true);
    int acceptedCount = 0;
    acceptor.setNewConnectionCallback([&acceptedCount](int sockfd, const InetAddress &) {
        ++acceptedCount;
        ::close(sockfd);
    });
    acceptor.listen();

    uint16_t port = getSocketPort(acceptor.acceptSocket_.fd());
    std::vector<int> clients;
    for (int i = 0; i < 3; ++i)
    {
        clients.push_back(createClient(port));
    }

    usleep(10000);
    acceptor.handleRead();
    EXPECT_EQ(acceptedCount, 3);

    closeAll(clients);
}

TEST(ETIOTest, AcceptorKeepsSingleAcceptBehaviorInLTMode)
{
    EventLoop loop("LT");
    Acceptor acceptor(&loop, InetAddress(0, "127.0.0.1"), false, false);
    int acceptedCount = 0;
    acceptor.setNewConnectionCallback([&acceptedCount](int sockfd, const InetAddress &) {
        ++acceptedCount;
        ::close(sockfd);
    });
    acceptor.listen();

    uint16_t port = getSocketPort(acceptor.acceptSocket_.fd());
    std::vector<int> clients;
    for (int i = 0; i < 3; ++i)
    {
        clients.push_back(createClient(port));
    }

    usleep(10000);
    acceptor.handleRead();
    EXPECT_EQ(acceptedCount, 1);

    acceptor.handleRead();
    acceptor.handleRead();
    closeAll(clients);
}

TEST(ETIOTest, TcpConnectionDrainsReadableDataInETMode)
{
    EventLoop loop("ET");
    int serverFd = -1;
    int clientFd = -1;
    InetAddress localAddr;
    InetAddress peerAddr;
    createSocketPair(&serverFd, &clientFd, &localAddr, &peerAddr);

    auto conn = createConnection(&loop, "ET", serverFd, localAddr, peerAddr);

    std::string payload(96 * 1024, 'x');
    writeAll(clientFd, payload);
    waitUntilReadable(serverFd);

    int callbackCount = 0;
    size_t bytesSeen = 0;
    conn->setMessageCallback([&callbackCount, &bytesSeen](const TcpConnectionPtr &, Buffer *buf, Timestamp) {
        ++callbackCount;
        bytesSeen = buf->readableBytes();
        buf->retrieveAll();
    });

    conn->handleRead(Timestamp::now());
    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(bytesSeen, payload.size());

    conn->connectDestroyed();
    conn.reset();
    ::close(clientFd);
}

TEST(ETIOTest, TcpConnectionKeepsSingleReadBehaviorInLTMode)
{
    EventLoop loop("LT");
    int serverFd = -1;
    int clientFd = -1;
    InetAddress localAddr;
    InetAddress peerAddr;
    createSocketPair(&serverFd, &clientFd, &localAddr, &peerAddr);

    auto conn = createConnection(&loop, "LT", serverFd, localAddr, peerAddr);

    std::string payload(96 * 1024, 'y');
    writeAll(clientFd, payload);
    waitUntilReadable(serverFd);

    int callbackCount = 0;
    size_t bytesSeen = 0;
    conn->setMessageCallback([&callbackCount, &bytesSeen](const TcpConnectionPtr &, Buffer *buf, Timestamp) {
        ++callbackCount;
        bytesSeen = buf->readableBytes();
        buf->retrieveAll();
    });

    conn->handleRead(Timestamp::now());
    EXPECT_EQ(callbackCount, 1);
    EXPECT_LT(bytesSeen, payload.size());
    EXPECT_GT(bytesSeen, 0u);

    conn->connectDestroyed();
    conn.reset();
    ::close(clientFd);
}

TEST(ETIOTest, HandleWriteClearsOutputBufferAndDisablesWriting)
{
    EventLoop loop("ET");
    int serverFd = -1;
    int clientFd = -1;
    InetAddress localAddr;
    InetAddress peerAddr;
    createSocketPair(&serverFd, &clientFd, &localAddr, &peerAddr);

    auto conn = createConnection(&loop, "ET", serverFd, localAddr, peerAddr);
    std::string payload(4096, 'z');
    conn->outputBuffer_.append(payload.data(), payload.size());
    conn->channel_->enableWriting();

    conn->handleWrite();
    EXPECT_EQ(conn->outputBuffer_.readableBytes(), 0u);
    EXPECT_FALSE(conn->channel_->isWriting());

    std::string received = readAll(clientFd, payload.size());
    EXPECT_EQ(received, payload);

    conn->connectDestroyed();
    conn.reset();
    ::close(clientFd);
}

TEST(ETIOTest, SendFileRestoresReadingAfterCompletion)
{
    EventLoop loop("ET");
    int serverFd = -1;
    int clientFd = -1;
    InetAddress localAddr;
    InetAddress peerAddr;
    createTcpPair(&serverFd, &clientFd, &localAddr, &peerAddr);

    auto conn = createConnection(&loop, "ET", serverFd, localAddr, peerAddr);

    std::string filePath = "/tmp/webserver_et_io_test.txt";
    std::string content = "sendfile-et-test";
    {
        std::ofstream out(filePath);
        ASSERT_TRUE(out.is_open());
        out << content;
    }

    bool completed = false;
    conn->sendFile(filePath, false, [&completed]() { completed = true; });

    EXPECT_TRUE(completed);
    EXPECT_TRUE(conn->channel_->isReading());
    EXPECT_EQ(conn->fileSendState_, nullptr);

    std::string received = readAll(clientFd, content.size());
    EXPECT_EQ(received, content);

    ::unlink(filePath.c_str());
    conn->connectDestroyed();
    conn.reset();
    ::close(clientFd);
}
