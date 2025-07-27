#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 网络配置类
 * @details 管理网络相关的配置参数，负责解析和提供'network'部分的配置
 */
class NetworkConfig : noncopyable
{
public:
    /**
     * @brief 构造函数
     * @param node 包含网络配置的YAML节点
     */
    explicit NetworkConfig(const YAML::Node &node);

    /**
     * @brief 获取服务器IP地址
     * @return IP地址字符串
     */
    std::string getIp() const;

    /**
     * @brief 获取服务器端口号
     * @return 端口号
     */
    int getPort() const;

    /**
     * @brief 获取线程池线程数量
     * @return 线程数量
     */
    int getThreadNum() const;

    /**
     * @brief 获取线程池队列大小
     * @return 队列大小
     */
    int getThreadPoolQueueSize() const;

    /**
     * @brief 获取线程池保活时间
     * @return 保活时间（秒）
     */
    int getThreadPoolKeepAliveTime() const;

    /**
     * @brief 获取线程池最大空闲线程数
     * @return 最大空闲线程数
     */
    int getThreadPoolMaxIdleThreads() const;

    /**
     * @brief 获取线程池最小空闲线程数
     * @return 最小空闲线程数
     */
    int getThreadPoolMinIdleThreads() const;

    /**
     * @brief 获取epoll模式
     * @return epoll模式字符串
     */
    std::string getEpollMode() const;

    /**
     * @brief 检查是否使用ET模式
     * @return true表示使用ET模式，false表示使用LT模式
     */
    bool isET() const;

    /**
     * @brief 获取空闲超时时间
     * @return 空闲超时时间（秒）
     */
    int getIdleTimeout() const;

    /**
     * @brief 检查是否启用SSL
     * @return true表示启用SSL，false表示不启用
     */
    bool isSSLEnabled() const;

    /**
     * @brief 获取SSL证书路径
     * @return SSL证书文件路径
     */
    std::string getSSLCertPath() const;

    /**
     * @brief 获取SSL私钥路径
     * @return SSL私钥文件路径
     */
    std::string getSSLKeyPath() const;

private:
    /**
     * @brief 验证配置参数的有效性
     * @param ip IP地址
     * @param port 端口号
     * @param threadNum 线程数量
     * @param queueSize 队列大小
     * @param keepAliveTime 保活时间
     * @param maxIdleThreads 最大空闲线程数
     * @param minIdleThreads 最小空闲线程数
     */
    void validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                        int keepAliveTime, int maxIdleThreads, int minIdleThreads);

    YAML::Node node_;              // YAML配置节点
    std::string epollMode_ = "LT"; // epoll模式，默认为LT
    int idleTimeout_ = 30;         // 空闲超时时间（秒）
};