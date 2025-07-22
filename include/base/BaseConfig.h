#pragma once
#include <yaml-cpp/yaml.h>
#include <string>

/**
 * @brief 基础配置类,提供配置管理的抽象接口
 *
 * 作为所有配置类的基类，定义了统一的配置加载、访问接口。
 * 支持单例模式，基于YAML-CPP解析配置文件。
 */
class BaseConfig
{
public:
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    virtual ~BaseConfig() = default;
    /**
     * @brief 加载配置文件的虚函数
     * @param filename 配置文件名
     * @throws std::invalid_argument 配置无效时抛出
     */
    virtual void load(const std::string &filename) = 0;
    /**
     * @brief 获取Buffer初始大小
     * @return Buffer的初始大小(字节)
     */
    int getBufferInitialSize() const;
    /**
     * @brief 获取Buffer最大大小
     * @return Buffer的最大大小(字节)
     */
    int getBufferMaxSize() const;
    /**
     * @brief 获取Buffer增长因子
     * @return Buffer的增长因子
     */
    int getBufferGrowthFactor() const;
    /**
     * @brief 获取JWT签名密钥
     * @return JWT密钥字符串
     */
    std::string getJwtSecret() const;
    /**
     * @brief 获取JWT过期时间（秒）
     * @return 过期秒数
     */
    int getJwtExpireSeconds() const;
    /**
     * @brief 获取JWT签发者
     * @return issuer字符串
     */
    std::string getJwtIssuer() const;
    /**
     * @brief 获取BaseConfig单例实例
     * @return BaseConfig的引用
     */
    static BaseConfig &getInstance();
    /**
     * @brief 获取配置节点
     * @return YAML配置节点的引用
     */
    const YAML::Node &getConfigNode() const { return config_; }

protected:
    YAML::Node config_;
    /**
     * @brief 保护构造函数，单例模式
     */
    BaseConfig() = default;
};