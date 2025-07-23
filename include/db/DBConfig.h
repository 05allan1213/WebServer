#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 数据库配置类,管理数据库相关的配置参数
 *
 * 负责解析和提供 'database' 部分的配置。
 */
class DBConfig : noncopyable
{
public:
    /**
     * @brief 构造函数, 从 YAML 节点解析配置
     * @param node 包含数据库配置的 YAML 节点
     */
    explicit DBConfig(const YAML::Node &node);

    std::string getHost() const;
    std::string getUser() const;
    std::string getPassword() const;
    std::string getDBName() const;
    int getPort() const;
    unsigned int getInitSize() const;
    unsigned int getMaxSize() const;
    int getMaxIdleTime() const;
    int getConnectionTimeout() const;
    bool isValid() const;

private:
    void validateConfig() const;
    YAML::Node node_;
    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_dbName;
    int m_port;
    unsigned int m_initSize;
    unsigned int m_maxSize;
    int m_maxIdleTime;
    int m_connectionTimeout;
};