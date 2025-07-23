#pragma once
#include <yaml-cpp/yaml.h>
#include <string>

/**
 * @brief 基础配置类, 提供通用的配置参数
 */
class BaseConfig
{
public:
    /**
     * @brief 构造函数, 从 YAML 根节点解析配置
     * @param rootNode 整个配置文件的 YAML 根节点
     */
    explicit BaseConfig(const YAML::Node &rootNode);

    /**
     * @brief 虚析构函数
     */
    virtual ~BaseConfig() = default;

    // ... (getBuffer* 方法声明不变) ...
    int getBufferInitialSize() const;
    int getBufferMaxSize() const;
    int getBufferGrowthFactor() const;

    // ... (getJwt* 方法声明不变) ...
    std::string getJwtSecret() const;
    int getJwtExpireSeconds() const;
    std::string getJwtIssuer() const;

private:
    void validateConfig(int initialSize, int maxSize, int growthFactor);

protected:
    YAML::Node rootNode_; // 保存整个根节点
    YAML::Node node_;     // 保存 "base" 子节点
};