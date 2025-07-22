#include "net/NetworkConfig.h"
#include "log/Log.h"

NetworkConfig &NetworkConfig::getInstance()
{
    static NetworkConfig instance;
    return instance;
}

void NetworkConfig::load(const std::string &filename)
{
    DLOG_INFO << "[NetworkConfig] 开始加载网络配置文件: " << filename;
    BaseConfig::getInstance().load(filename);
    DLOG_INFO << "[NetworkConfig] BaseConfig 加载完成，开始读取网络配置参数";
    const YAML::Node &config = BaseConfig::getInstance().getConfigNode();
    std::string ip = config["network"]["ip"] ? config["network"]["ip"].as<std::string>() : "127.0.0.1";
    int port = config["network"]["port"] ? config["network"]["port"].as<int>() : 8080;
    int threadNum = config["network"]["thread_pool"] && config["network"]["thread_pool"]["thread_num"] ? config["network"]["thread_pool"]["thread_num"].as<int>() : 3;
    int queueSize = config["network"]["thread_pool"] && config["network"]["thread_pool"]["queue_size"] ? config["network"]["thread_pool"]["queue_size"].as<int>() : 1000;
    int keepAliveTime = config["network"]["thread_pool"] && config["network"]["thread_pool"]["keep_alive_time"] ? config["network"]["thread_pool"]["keep_alive_time"].as<int>() : 60;
    int maxIdleThreads = config["network"]["thread_pool"] && config["network"]["thread_pool"]["max_idle_threads"] ? config["network"]["thread_pool"]["max_idle_threads"].as<int>() : 5;
    int minIdleThreads = config["network"]["thread_pool"] && config["network"]["thread_pool"]["min_idle_threads"] ? config["network"]["thread_pool"]["min_idle_threads"].as<int>() : 1;
    std::string epollMode = config["network"]["epoll_mode"] ? config["network"]["epoll_mode"].as<std::string>() : "LT";
    int idleTimeout = config["network"]["idle_timeout"] ? config["network"]["idle_timeout"].as<int>() : 30;
    DLOG_INFO << "[NetworkConfig] 读取到配置: ip=" << ip
              << ", port=" << port
              << ", thread_num=" << threadNum
              << ", queue_size=" << queueSize
              << ", keep_alive_time=" << keepAliveTime
              << ", max_idle_threads=" << maxIdleThreads
              << ", min_idle_threads=" << minIdleThreads
              << ", epoll_mode=" << epollMode
              << ", idle_timeout=" << idleTimeout;
    validateConfig(ip, port, threadNum, queueSize, keepAliveTime, maxIdleThreads, minIdleThreads);
    DLOG_INFO << "[NetworkConfig] 网络配置校验通过";
    epollMode_ = epollMode;
    idleTimeout_ = idleTimeout;
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
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["ip"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.ip 缺失，使用默认值 127.0.0.1";
        return "127.0.0.1";
    }
    return node["network"]["ip"].as<std::string>();
}

int NetworkConfig::getPort() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["port"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.port 缺失，使用默认值 8080";
        return 8080;
    }
    return node["network"]["port"].as<int>();
}

int NetworkConfig::getThreadNum() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["thread_pool"] || !node["network"]["thread_pool"]["thread_num"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.thread_num 缺失，使用默认值 3";
        return 3;
    }
    return node["network"]["thread_pool"]["thread_num"].as<int>();
}

int NetworkConfig::getThreadPoolQueueSize() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["thread_pool"] || !node["network"]["thread_pool"]["queue_size"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.queue_size 缺失，使用默认值 1000";
        return 1000;
    }
    return node["network"]["thread_pool"]["queue_size"].as<int>();
}

int NetworkConfig::getThreadPoolKeepAliveTime() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["thread_pool"] || !node["network"]["thread_pool"]["keep_alive_time"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.keep_alive_time 缺失，使用默认值 60";
        return 60;
    }
    return node["network"]["thread_pool"]["keep_alive_time"].as<int>();
}

int NetworkConfig::getThreadPoolMaxIdleThreads() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["thread_pool"] || !node["network"]["thread_pool"]["max_idle_threads"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.max_idle_threads 缺失，使用默认值 5";
        return 5;
    }
    return node["network"]["thread_pool"]["max_idle_threads"].as<int>();
}

int NetworkConfig::getThreadPoolMinIdleThreads() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["network"] || !node["network"]["thread_pool"] || !node["network"]["thread_pool"]["min_idle_threads"])
    {
        DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.min_idle_threads 缺失，使用默认值 1";
        return 1;
    }
    return node["network"]["thread_pool"]["min_idle_threads"].as<int>();
}

std::string NetworkConfig::getEpollMode() const
{
    return epollMode_;
}

bool NetworkConfig::isET() const
{
    return epollMode_ == "ET";
}

int NetworkConfig::getIdleTimeout() const
{
    return idleTimeout_;
}