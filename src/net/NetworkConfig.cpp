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
    config_ = YAML::LoadFile(filename);
    DLOG_INFO << "NetworkConfig: 配置文件加载完成";

    // 读取并记录配置值
    std::string ip = config_["network"]["ip"].as<std::string>();
    int port = config_["network"]["port"].as<int>();
    int threadNum = config_["network"]["thread_pool"]["thread_num"].as<int>();
    int queueSize = config_["network"]["thread_pool"]["queue_size"].as<int>();
    int keepAliveTime = config_["network"]["thread_pool"]["keep_alive_time"].as<int>();
    int maxIdleThreads = config_["network"]["thread_pool"]["max_idle_threads"].as<int>();
    int minIdleThreads = config_["network"]["thread_pool"]["min_idle_threads"].as<int>();

    DLOG_INFO << "NetworkConfig: 读取到配置 - ip=" << ip
              << ", port=" << port
              << ", thread_num=" << threadNum
              << ", queue_size=" << queueSize
              << ", keep_alive_time=" << keepAliveTime
              << ", max_idle_threads=" << maxIdleThreads
              << ", min_idle_threads=" << minIdleThreads;
}

std::string NetworkConfig::getIp() const
{
    return config_["network"]["ip"].as<std::string>();
}

int NetworkConfig::getPort() const
{
    return config_["network"]["port"].as<int>();
}

int NetworkConfig::getThreadNum() const
{
    return config_["network"]["thread_pool"]["thread_num"].as<int>();
}

int NetworkConfig::getThreadPoolQueueSize() const
{
    return config_["network"]["thread_pool"]["queue_size"].as<int>();
}

int NetworkConfig::getThreadPoolKeepAliveTime() const
{
    return config_["network"]["thread_pool"]["keep_alive_time"].as<int>();
}

int NetworkConfig::getThreadPoolMaxIdleThreads() const
{
    return config_["network"]["thread_pool"]["max_idle_threads"].as<int>();
}

int NetworkConfig::getThreadPoolMinIdleThreads() const
{
    return config_["network"]["thread_pool"]["min_idle_threads"].as<int>();
}