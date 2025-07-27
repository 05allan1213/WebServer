#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 数据库配置类，管理数据库相关的配置参数
 *
 * 负责解析和提供database部分的配置，包括连接参数和连接池配置。
 */
class DBConfig : noncopyable
{
public:
    /**
     * @brief 构造函数，从YAML节点解析配置
     * @param node 包含数据库配置的YAML节点
     */
    explicit DBConfig(const YAML::Node &node);

    /**
     * @brief 获取数据库主机地址
     * @return 主机地址字符串
     */
    std::string getHost() const;

    /**
     * @brief 获取数据库用户名
     * @return 用户名字符串，默认"root"
     */
    std::string getUser() const;

    /**
     * @brief 获取数据库密码
     * @return 密码字符串
     */
    std::string getPassword() const;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称字符串
     */
    std::string getDBName() const;

    /**
     * @brief 获取数据库端口号
     * @return 端口号，默认0
     */
    int getPort() const;

    /**
     * @brief 获取连接池初始大小
     * @return 初始连接数，默认0
     */
    unsigned int getInitSize() const;

    /**
     * @brief 获取连接池最大大小
     * @return 最大连接数，默认0
     */
    unsigned int getMaxSize() const;

    /**
     * @brief 获取最大空闲时间
     * @return 最大空闲时间（秒），默认0
     */
    int getMaxIdleTime() const;

    /**
     * @brief 获取连接超时时间
     * @return 连接超时时间（毫秒），默认0
     */
    int getConnectionTimeout() const;

    /**
     * @brief 检查配置是否有效
     * @return 配置是否有效
     */
    bool isValid() const;

private:
    /**
     * @brief 验证配置参数的有效性
     */
    void validateConfig() const;

    YAML::Node node_;        // YAML配置节点
    std::string m_host;      // 数据库主机地址
    std::string m_user;      // 数据库用户名
    std::string m_password;  // 数据库密码
    std::string m_dbName;    // 数据库名称
    int m_port;              // 数据库端口号
    unsigned int m_initSize; // 连接池初始大小
    unsigned int m_maxSize;  // 连接池最大大小
    int m_maxIdleTime;       // 最大空闲时间
    int m_connectionTimeout; // 连接超时时间
};