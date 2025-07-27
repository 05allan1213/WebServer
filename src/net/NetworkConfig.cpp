#include "net/NetworkConfig.h"
#include "log/Log.h"

/**
 * @brief 构造函数
 * @param node 包含网络配置的YAML节点
 * @details 从YAML节点解析网络配置参数，包括IP、端口、线程池配置等，并进行配置验证
 */
NetworkConfig::NetworkConfig(const YAML::Node &node) : node_(node)
{
    DLOG_INFO << "[NetworkConfig] 开始解析 'network' 配置...";
    try
    {
        // 获取基本配置参数
        std::string ip = getIp();
        int port = getPort();
        int threadNum = getThreadNum();
        int queueSize = getThreadPoolQueueSize();
        int keepAliveTime = getThreadPoolKeepAliveTime();
        int maxIdleThreads = getThreadPoolMaxIdleThreads();
        int minIdleThreads = getThreadPoolMinIdleThreads();

        // 获取epoll模式和空闲超时配置
        epollMode_ = node_["epoll_mode"] ? node_["epoll_mode"].as<std::string>() : "LT";
        idleTimeout_ = node_["idle_timeout"] ? node_["idle_timeout"].as<int>() : 30;

        // 验证所有配置参数的有效性
        validateConfig(ip, port, threadNum, queueSize, keepAliveTime, maxIdleThreads, minIdleThreads);
        DLOG_INFO << "[NetworkConfig] 网络配置校验通过";
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "[NetworkConfig] 配置解析或验证失败: " << e.what();
        throw;
    }
}

/**
 * @brief 验证配置参数的有效性
 * @param ip IP地址
 * @param port 端口号
 * @param threadNum 线程数量
 * @param queueSize 队列大小
 * @param keepAliveTime 保活时间
 * @param maxIdleThreads 最大空闲线程数
 * @param minIdleThreads 最小空闲线程数
 * @details 对网络配置的各个参数进行范围检查和逻辑验证，确保配置的合理性
 */
void NetworkConfig::validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                                   int keepAliveTime, int maxIdleThreads, int minIdleThreads)
{
    DLOG_INFO << "NetworkConfig: 开始验证配置...";

    // 验证IP地址：不能为空
    if (ip.empty())
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.ip不能为空";
        throw std::invalid_argument("network.ip不能为空");
    }

    // 验证端口号：必须在有效范围内（1024-65535）
    if (port < 1024 || port > 65535)
    {
        DLOG_ERROR << "NetworkConfig: 配置验证失败 - network.port必须在1024-65535之间，当前值: " << port;
        throw std::invalid_argument("network.port必须在1024-65535之间");
    }

    // 验证线程数：必须大于0且不超过32
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

    // 验证队列大小：必须大于0且不超过10000
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

    // 验证保活时间：必须在合理范围内（1-3600秒）
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

    // 验证最大空闲线程数：必须大于0且不小于线程总数
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

    // 验证最小空闲线程数：必须大于0且不能超过最大空闲线程数和线程总数
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

/**
 * @brief 获取服务器IP地址
 * @return IP地址字符串
 * @details 从YAML配置中读取IP地址，如果未配置则返回默认值127.0.0.1
 */
std::string NetworkConfig::getIp() const
{
    if (node_ && node_["ip"])
    {
        return node_["ip"].as<std::string>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.ip 缺失，使用默认值 127.0.0.1";
    return "127.0.0.1";
}

/**
 * @brief 获取服务器端口号
 * @return 端口号
 * @details 从YAML配置中读取端口号，如果未配置则返回默认值8080
 */
int NetworkConfig::getPort() const
{
    if (node_ && node_["port"])
    {
        return node_["port"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.port 缺失，使用默认值 8080";
    return 8080;
}

/**
 * @brief 获取线程池线程数量
 * @return 线程数量
 * @details 从YAML配置中读取线程池线程数，如果未配置则返回默认值3
 */
int NetworkConfig::getThreadNum() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["thread_num"])
    {
        return node_["thread_pool"]["thread_num"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.thread_num 缺失，使用默认值 3";
    return 3;
}

/**
 * @brief 获取线程池队列大小
 * @return 队列大小
 * @details 从YAML配置中读取线程池队列大小，如果未配置则返回默认值1000
 */
int NetworkConfig::getThreadPoolQueueSize() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["queue_size"])
    {
        return node_["thread_pool"]["queue_size"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.queue_size 缺失，使用默认值 1000";
    return 1000;
}

/**
 * @brief 获取线程池保活时间
 * @return 保活时间（秒）
 * @details 从YAML配置中读取线程池保活时间，如果未配置则返回默认值60秒
 */
int NetworkConfig::getThreadPoolKeepAliveTime() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["keep_alive_time"])
    {
        return node_["thread_pool"]["keep_alive_time"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.keep_alive_time 缺失，使用默认值 60";
    return 60;
}

/**
 * @brief 获取线程池最大空闲线程数
 * @return 最大空闲线程数
 * @details 从YAML配置中读取最大空闲线程数，如果未配置则返回默认值5
 */
int NetworkConfig::getThreadPoolMaxIdleThreads() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["max_idle_threads"])
    {
        return node_["thread_pool"]["max_idle_threads"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.max_idle_threads 缺失，使用默认值 5";
    return 5;
}

/**
 * @brief 获取线程池最小空闲线程数
 * @return 最小空闲线程数
 * @details 从YAML配置中读取最小空闲线程数，如果未配置则返回默认值1
 */
int NetworkConfig::getThreadPoolMinIdleThreads() const
{
    if (node_ && node_["thread_pool"] && node_["thread_pool"]["min_idle_threads"])
    {
        return node_["thread_pool"]["min_idle_threads"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.thread_pool.min_idle_threads 缺失，使用默认值 1";
    return 1;
}

/**
 * @brief 获取epoll模式
 * @return epoll模式字符串
 * @details 从YAML配置中读取epoll模式，如果未配置则返回默认值LT
 */
std::string NetworkConfig::getEpollMode() const
{
    if (node_ && node_["epoll_mode"])
    {
        return node_["epoll_mode"].as<std::string>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.epoll_mode 缺失，使用默认值 LT";
    return "LT";
}

/**
 * @brief 检查是否使用ET模式
 * @return true表示使用ET模式，false表示使用LT模式
 * @details 通过比较epoll模式字符串来判断是否使用边缘触发模式
 */
bool NetworkConfig::isET() const
{
    return getEpollMode() == "ET";
}

/**
 * @brief 获取空闲超时时间
 * @return 空闲超时时间（秒）
 * @details 从YAML配置中读取空闲超时时间，如果未配置则返回默认值30秒
 */
int NetworkConfig::getIdleTimeout() const
{
    if (node_ && node_["idle_timeout"])
    {
        return node_["idle_timeout"].as<int>();
    }
    DLOG_WARN << "[NetworkConfig] 配置项 network.idle_timeout 缺失，使用默认值 30";
    return 30;
}

/**
 * @brief 检查是否启用SSL
 * @return true表示启用SSL，false表示不启用
 * @details 从YAML配置中读取SSL启用状态，如果未配置则返回false
 */
bool NetworkConfig::isSSLEnabled() const
{
    if (node_ && node_["ssl"] && node_["ssl"]["enable"])
    {
        return node_["ssl"]["enable"].as<bool>();
    }
    return false;
}

/**
 * @brief 获取SSL证书路径
 * @return SSL证书文件路径
 * @details 从YAML配置中读取SSL证书路径，如果未配置则返回空字符串
 */
std::string NetworkConfig::getSSLCertPath() const
{
    if (node_ && node_["ssl"] && node_["ssl"]["cert_path"])
    {
        return node_["ssl"]["cert_path"].as<std::string>();
    }
    return "";
}

/**
 * @brief 获取SSL私钥路径
 * @return SSL私钥文件路径
 * @details 从YAML配置中读取SSL私钥路径，如果未配置则返回空字符串
 */
std::string NetworkConfig::getSSLKeyPath() const
{
    if (node_ && node_["ssl"] && node_["ssl"]["key_path"])
    {
        return node_["ssl"]["key_path"].as<std::string>();
    }
    return "";
}