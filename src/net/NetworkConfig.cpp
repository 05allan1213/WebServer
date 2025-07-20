#include "net/NetworkConfig.h"
#include "log/Log.h"

NetworkConfig &NetworkConfig::getInstance()
{
    static NetworkConfig instance;
    return instance;
}

void NetworkConfig::load(const std::string &filename)
{
    DLOG_INFO << "NetworkConfig: 开始加载配置文件 " << filename;

    // 调用BaseConfig的load方法加载配置文件
    BaseConfig::getInstance().load(filename);

    // 使用BaseConfig的配置节点
    const YAML::Node &config = BaseConfig::getInstance().getConfigNode();

    // 读取并记录配置值
    std::string ip = config["network"]["ip"].as<std::string>();
    int port = config["network"]["port"].as<int>();
    int threadNum = config["network"]["thread_pool"]["thread_num"].as<int>();
    int queueSize = config["network"]["thread_pool"]["queue_size"].as<int>();
    int keepAliveTime = config["network"]["thread_pool"]["keep_alive_time"].as<int>();
    int maxIdleThreads = config["network"]["thread_pool"]["max_idle_threads"].as<int>();
    int minIdleThreads = config["network"]["thread_pool"]["min_idle_threads"].as<int>();

    DLOG_INFO << "NetworkConfig: 读取到配置 - ip=" << ip
              << ", port=" << port
              << ", thread_num=" << threadNum
              << ", queue_size=" << queueSize
              << ", keep_alive_time=" << keepAliveTime
              << ", max_idle_threads=" << maxIdleThreads
              << ", min_idle_threads=" << minIdleThreads;

    // 验证配置
    validateConfig(ip, port, threadNum, queueSize, keepAliveTime, maxIdleThreads, minIdleThreads);
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
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.port必须在1024-65535之间，当前值: " << port;
        throw std::invalid_argument("network.port必须在1024-65535之间");
    }

    // 验证线程数
    if (threadNum <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.thread_num必须大于0，当前值: " << threadNum;
        throw std::invalid_argument("network.thread_pool.thread_num必须大于0");
    }

    if (threadNum > 32)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.thread_num不能超过32，当前值: " << threadNum;
        throw std::invalid_argument("network.thread_pool.thread_num不能超过32");
    }

    // 验证队列大小
    if (queueSize <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.queue_size必须大于0，当前值: " << queueSize;
        throw std::invalid_argument("network.thread_pool.queue_size必须大于0");
    }

    if (queueSize > 10000)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.queue_size不能超过10000，当前值: " << queueSize;
        throw std::invalid_argument("network.thread_pool.queue_size不能超过10000");
    }

    // 验证保活时间
    if (keepAliveTime <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.keep_alive_time必须大于0，当前值: " << keepAliveTime;
        throw std::invalid_argument("network.thread_pool.keep_alive_time必须大于0");
    }

    if (keepAliveTime > 3600)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.keep_alive_time不能超过3600秒，当前值: " << keepAliveTime;
        throw std::invalid_argument("network.thread_pool.keep_alive_time不能超过3600秒");
    }

    // 验证最大空闲线程数
    if (maxIdleThreads <= 0)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.max_idle_threads必须大于0，当前值: " << maxIdleThreads;
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
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.thread_pool.min_idle_threads必须大于0，当前值: " << minIdleThreads;
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
    return BaseConfig::getInstance().getConfigNode()["network"]["ip"].as<std::string>();
}

int NetworkConfig::getPort() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["port"].as<int>();
}

int NetworkConfig::getThreadNum() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["thread_pool"]["thread_num"].as<int>();
}

int NetworkConfig::getThreadPoolQueueSize() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["thread_pool"]["queue_size"].as<int>();
}

int NetworkConfig::getThreadPoolKeepAliveTime() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["thread_pool"]["keep_alive_time"].as<int>();
}

int NetworkConfig::getThreadPoolMaxIdleThreads() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["thread_pool"]["max_idle_threads"].as<int>();
}

int NetworkConfig::getThreadPoolMinIdleThreads() const
{
    return BaseConfig::getInstance().getConfigNode()["network"]["thread_pool"]["min_idle_threads"].as<int>();
}