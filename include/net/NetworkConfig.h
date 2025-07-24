#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 网络配置类,管理网络相关的配置参数
 *
 * 负责解析和提供 'network' 部分的配置。
 */
class NetworkConfig : noncopyable
{
public:
    /**
     * @brief 构造函数, 从 YAML 节点解析配置
     * @param node 包含网络配置的 YAML 节点
     */
    explicit NetworkConfig(const YAML::Node &node);

    std::string getIp() const;
    int getPort() const;
    int getThreadNum() const;
    int getThreadPoolQueueSize() const;
    int getThreadPoolKeepAliveTime() const;
    int getThreadPoolMaxIdleThreads() const;
    int getThreadPoolMinIdleThreads() const;
    std::string getEpollMode() const;
    bool isET() const;
    int getIdleTimeout() const;
    bool isSSLEnabled() const;
    std::string getSSLCertPath() const;
    std::string getSSLKeyPath() const;

private:
    void validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                        int keepAliveTime, int maxIdleThreads, int minIdleThreads);
    YAML::Node node_;
    std::string epollMode_ = "LT";
    int idleTimeout_ = 30;
};