#pragma once
#include "base/BaseConfig.h"

/**
 * @brief 网络配置类,管理网络相关的配置参数
 *
 * NetworkConfig继承自BaseConfig,专门管理网络库的配置参数。
 * 主要功能：
 * - 加载网络配置文件
 * - 提供网络基础配置接口(IP、端口)
 * - 提供线程池配置接口
 * - 配置参数的验证
 *
 * 设计特点：
 * - 单例模式,确保全局唯一的配置实例
 * - 继承BaseConfig,复用配置加载功能
 * - 提供类型安全的配置访问接口
 * - 支持配置参数的验证
 *
 * 配置参数包括：
 * - 网络基础配置：IP地址、端口号
 * - 线程池配置：线程数量、队列大小、保活时间等
 */
class NetworkConfig : public BaseConfig
{
public:
    /**
     * @brief 加载配置文件
     * @param filename 配置文件名
     *
     * 重写基类方法,加载网络配置文件并验证参数
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
     */
    int getIdleTimeout() const;

    /**
     * @brief 获取NetworkConfig单例实例
     * @return NetworkConfig的引用
     *
     * 单例模式的访问接口,确保全局唯一的配置实例
     */
    static NetworkConfig &getInstance();

private:
    /**
     * @brief 私有构造函数
     *
     * 单例模式,禁止外部创建实例
     */
    NetworkConfig() = default;

    /** @brief 单例实例 */
    static NetworkConfig instance_;

    /**
     * @brief 配置参数验证方法
     * @param ip IP地址
     * @param port 端口号
     * @param threadNum 线程数量
     * @param queueSize 队列大小
     * @param keepAliveTime 保活时间
     * @param maxIdleThreads 最大空闲线程数
     * @param minIdleThreads 最小空闲线程数
     *
     * 验证配置参数的有效性,如果参数无效会抛出异常
     */
    void validateConfig(const std::string &ip, int port, int threadNum, int queueSize,
                        int keepAliveTime, int maxIdleThreads, int minIdleThreads);

    std::string epollMode_ = "LT"; // epoll触发模式,ET=边缘触发,LT=水平触发
    int idleTimeout_ = 30;         // 默认30秒
};