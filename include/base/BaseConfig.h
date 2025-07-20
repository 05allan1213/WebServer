#pragma once
#include <yaml-cpp/yaml.h>
#include <string>

/**
 * @brief 基础配置类，提供配置管理的抽象接口
 *
 * BaseConfig是配置系统的抽象基类，定义了配置管理的基本接口。
 * 主要功能：
 * - 提供配置加载的抽象接口
 * - 管理Buffer相关的配置参数
 * - 使用YAML格式存储配置数据
 * - 支持配置的验证和访问
 *
 * 设计特点：
 * - 采用单例模式，确保全局唯一的配置实例
 * - 使用虚函数提供多态接口
 * - 基于YAML-CPP库解析配置文件
 * - 提供类型安全的配置访问方法
 *
 * 配置参数包括：
 * - Buffer配置：初始大小、最大大小、增长因子
 * - 支持扩展其他模块的配置
 *
 * 使用场景：
 * - 作为其他配置类的基类
 * - 需要统一的配置管理接口时
 * - 需要Buffer相关配置时
 */
class BaseConfig
{
public:
    /**
     * @brief 虚析构函数
     *
     * 确保派生类能够正确析构
     */
    virtual ~BaseConfig() = default;

    /**
     * @brief 加载配置文件的虚函数
     * @param filename 配置文件名
     *
     * 纯虚函数，由派生类实现具体的配置加载逻辑
     */
    virtual void load(const std::string &filename) = 0;

    /**
     * @brief 获取Buffer初始大小
     * @return Buffer的初始大小（字节）
     */
    int getBufferInitialSize() const;

    /**
     * @brief 获取Buffer最大大小
     * @return Buffer的最大大小（字节）
     */
    int getBufferMaxSize() const;

    /**
     * @brief 获取Buffer增长因子
     * @return Buffer的增长因子
     *
     * 当Buffer需要扩容时，新大小 = 当前大小 * 增长因子
     */
    int getBufferGrowthFactor() const;

    /**
     * @brief 获取BaseConfig单例实例
     * @return BaseConfig的引用
     *
     * 单例模式的访问接口，确保全局唯一的配置实例
     */
    static BaseConfig &getInstance();

    /**
     * @brief 获取配置节点
     * @return YAML配置节点的引用
     *
     * 允许派生类访问配置节点
     */
    const YAML::Node &getConfigNode() const { return config_; }

protected:
    YAML::Node config_; /**< YAML配置节点，存储解析后的配置数据 */

    /**
     * @brief 保护构造函数
     *
     * 单例模式，禁止外部创建实例
     */
    BaseConfig() = default;
};