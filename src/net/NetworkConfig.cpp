#include "net/NetworkConfig.h"
#include "log/Log.h"

NetworkConfig::NetworkConfig(const YAML::Node &node) : node_(node)
{
    DLOG_INFO << "[NetworkConfig] 开始解析 'network' 配置...";
    try
    {
        std::string ip = getIp();
        int port = getPort();
        int threadNum = getThreadNum();
        int queueSize = getThreadPoolQueueSize();
        int keepAliveTime = getThreadPoolKeepAliveTime();
        int maxIdleThreads = getThreadPoolMaxIdleThreads();
        int minIdleThreads = getThreadPoolMinIdleThreads();

        epollMode_ = node_["epoll_mode"] ? node_["epoll_mode"].as<std::string>() : "LT";
        idleTimeout_ = node_["idle_timeout"] ? node_["idle_timeout"].as<int>() : 30;

        validateConfig(ip, port, threadNum, queueSize, keepAliveTime, maxIdleThreads, minIdleThreads);
        DLOG_INFO << "[NetworkConfig] 网络配置校验通过";
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "[NetworkConfig] 配置解析或验证失败: " << e.what();
        throw;
    }
}

void NetworkConfig::validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                                   int keepAliveTime, int maxIdleThreads, int minIdleThreads)
{
    DLOG_INFO << "NetworkConfig: 开始验证配置...";

    // 验证IP地址
    if (ip.empty())
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.ip不能为空";
        throw std::invalid_argument("network.ip不能为空");
    }

    // 验证端口号
    if (port < 1024 || port > 65535)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.port必须在1024-65535之间,当前值: " << port;
        throw std::invalid_argument("network.port必须在1024-65535之间");
    }

    // 验证线程数
    if (threadNum <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.thread_num必须大于0,当前值: " << threadNum;
        throw std::invalid_argument("network.thread_pool.thread_num必须大于0");
    }

    if (threadNum > 32)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.thread_num不能超过32,当前值: " << threadNum;
        throw std::invalid_argument("network.thread_pool.thread_num不能超过32");
    }

    // 验证队列大小
    if (queueSize <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.queue_size必须大于0,当前值: " << queueSize;
        throw std::invalid_argument("network.thread_pool.queue_size必须大于0");
    }

    if (queueSize > 10000)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.queue_size不能超过10000,当前值: " << queueSize;
        throw std::invalid_argument("network.thread_pool.queue_size不能超过10000");
    }

    // 验证保活时间
    if (keepAliveTime <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.keep_alive_time必须大于0,当前值: " << keepAliveTime;
        throw std::invalid_argument("network.thread_pool.keep_alive_time必须大于0");
    }

    if (keepAliveTime > 3600)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.keep_alive_time不能超过3600秒,当前值: " << keepAliveTime;
        throw std::invalid_argument("network.thread_pool.keep_alive_time不能超过3600秒");
    }

    // 验证最大空闲线程数
    if (maxIdleThreads <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.max_idle_threads必须大于0,当前值: " << maxIdleThreads;
        throw std::invalid_argument("network.thread_pool.max_idle_threads必须大于0");
    }

    if (maxIdleThreads < threadNum)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.max_idle_threads不能小于thread_num";
        throw std::invalid_argument("network.thread_pool.max_idle_threads不能小于thread_num");
    }

    // 验证最小空闲线程数
    if (minIdleThreads <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.min_idle_threads必须大于0,当前值: " << minIdleThreads;
        throw std::invalid_argument("network.thread_pool.min_idle_threads必须大于0");
    }

    if (minIdleThreads > maxIdleThreads)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.min_idle_threads不能大于max_idle_threads";
        throw std::invalid_argument("network.thread_pool.min_idle_threads不能大于max_idle_threads");
    }

    if (minIdleThreads > threadNum)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.min_idle_threads不能大于thread_num";
        throw std::invalid_argument("network.thread_pool.min_idle_threads不能大于thread_num");
    }

    DLOG_INFO << "NetworkConfig: 配置验证通过";
}

std::string NetworkConfig::getIp() const
{
    if (node_ && node_["ip"])
    {
        return node_["ip"].as<std::string>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.ip 缺失，使用默认值 127.0.0.1";
    return "127.0.0.1";
}

int NetworkConfig::getPort() const
{
    if (node_ && node_["port"])
    {
        return node_["port"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.port 缺失，使用默认值 8080";
    return 8080;
}

int NetworkConfig::getThreadNum() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["thread_num"])
    {
        return node_["thread_pool"]["thread_num"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.thread_num 缺失，使用默认值 3";
    return 3;
}

int NetworkConfig::getThreadPoolQueueSize() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["queue_size"])
    {
        return node_["thread_pool"]["queue_size"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.queue_size 缺失，使用默认值 1000";
    return 1000;
}

int NetworkConfig::getThreadPoolKeepAliveTime() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["keep_alive_time"])
    {
        return node_["thread_pool"]["keep_alive_time"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.keep_alive_time 缺失，使用默认值 60";
    return 60;
}

int NetworkConfig::getThreadPoolMaxIdleThreads() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["max_idle_threads"])
    {
        return node_["thread_pool"]["max_idle_threads"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.max_idle_threads 缺失，使用默认值 5";
    return 5;
}

int NetworkConfig::getThreadPoolMinIdleThreads() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["min_idle_threads"])
    {
        return node_["thread_pool"]["min_idle_threads"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.min_idle_threads 缺失，使用默认值 1";
    return 1;
}

std::string NetworkConfig::getEpollMode() const
{
    if (node_ && node_["epoll_mode"])
    {
        return node_["epoll_mode"].as<std::string>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.epoll_mode 缺失，使用默认值 LT";
    return "LT";
}

bool NetworkConfig::isET() const
{
    return getEpollMode() == "ET";
}

int NetworkConfig::getIdleTimeout() const
{
    if (node_ && node_["idle_timeout"])
    {
        return node_["idle_timeout"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.idle_timeout 缺失，使用默认值 30";
    return 30;
}