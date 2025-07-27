#pragma once
#include <yaml-cpp/yaml.h>
#include <string>

/**
 * @brief 基础配置类，提供通用的配置参数
 *
 * 负责解析和管理基础配置项，包括缓冲区配置和JWT相关配置。
 * 从YAML配置文件中读取base节点下的配置信息。
 */
class BaseConfig
{
public:
    /**
     * @brief 构造函数，从YAML根节点解析配置
     * @param rootNode 整个配置文件的YAML根节点
     */
    explicit BaseConfig(const YAML::Node &rootNode);

    /**
     * @brief 虚析构函数
     */
    virtual ~BaseConfig() = default;

    /**
     * @brief 获取缓冲区初始大小
     * @return 缓冲区初始大小，默认1024
     */
    int getBufferInitialSize() const;

    /**
     * @brief 获取缓冲区最大大小
     * @return 缓冲区最大大小，默认65536
     */
    int getBufferMaxSize() const;

    /**
     * @brief 获取缓冲区增长因子
     * @return 缓冲区增长因子，默认2
     */
    int getBufferGrowthFactor() const;

    /**
     * @brief 获取JWT密钥
     * @return JWT密钥字符串，默认"default_secret"
     */
    std::string getJwtSecret() const;

    /**
     * @brief 获取JWT过期时间
     * @return JWT过期时间（秒），默认86400
     */
    int getJwtExpireSeconds() const;

    /**
     * @brief 获取JWT发行者
     * @return JWT发行者字符串，默认"webserver"
     */
    std::string getJwtIssuer() const;

private:
    /**
     * @brief 验证配置参数的有效性
     * @param initialSize 初始大小
     * @param maxSize 最大大小
     * @param growthFactor 增长因子
     */
    void validateConfig(int initialSize, int maxSize, int growthFactor);

protected:
    YAML::Node rootNode_; // 保存整个根节点
    YAML::Node node_;     // 保存"base"子节点
};