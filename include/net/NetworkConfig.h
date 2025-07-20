#pragma once
#include "base/BaseConfig.h"

class NetworkConfig : public BaseConfig
{
public:
    void load(const std::string &filename) override;

    // 网络基础配置接口
    std::string getIp() const;
    int getPort() const;

    // 线程池配置接口
    int getThreadNum() const;
    int getThreadPoolQueueSize() const;
    int getThreadPoolKeepAliveTime() const;
    int getThreadPoolMaxIdleThreads() const;
    int getThreadPoolMinIdleThreads() const;

    static NetworkConfig &getInstance();

private:
    NetworkConfig() = default;
    static NetworkConfig instance_;

    // 配置验证方法
    void validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                        int keepAliveTime, int maxIdleThreads, int minIdleThreads);
};