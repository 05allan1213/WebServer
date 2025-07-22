#pragma once
#include "base/BaseConfig.h"

/**
 * @brief 网络配置类,管理网络相关的配置参数
 *
 * 负责加载、校验和访问网络相关配置项。
 * 支持单例模式，所有getter方法均支持缺省值和警告日志。
 *

 */
class NetworkConfig : public BaseConfig
{
public:
    /**
     * @brief 加载配置文件
     * @param filename 配置文件名
     * @throws std::invalid_argument 配置无效时抛出
     */
    void load(const std::string &filename) override;
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
     * @return 保活时间(秒)
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
     * @brief 获取epoll模式(ET/LT)
     * @return "ET" 或 "LT"
     */
    std::string getEpollMode() const;
    /**
     * @brief 是否为ET(边缘触发)模式
     * @return true=ET,false=LT
     */
    bool isET() const;
    /**
     * @brief 获取空闲连接超时时间(秒)
     * @return 超时时间(秒)
     */
    int getIdleTimeout() const;
    /**
     * @brief 获取NetworkConfig单例实例
     * @return NetworkConfig的引用
     */
    static NetworkConfig &getInstance();

private:
    /**
     * @brief 私有构造函数，单例模式
     */
    NetworkConfig() = default;
    /**
     * @brief 配置参数验证方法
     * @param ip IP地址
     * @param port 端口号
     * @param threadNum 线程数量
     * @param queueSize 队列大小
     * @param keepAliveTime 保活时间
     * @param maxIdleThreads 最大空闲线程数
     * @param minIdleThreads 最小空闲线程数
     * @throws std::invalid_argument 配置无效时抛出
     */
    void validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                        int keepAliveTime, int maxIdleThreads, int minIdleThreads);

    std::string epollMode_ = "LT"; // epoll触发模式,ET=边缘触发,LT=水平触发
    int idleTimeout_ = 30;         // 默认30秒
};