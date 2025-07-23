#include "db/DBConfig.h"
#include "log/Log.h"
#include <iostream>

DBConfig::DBConfig(const YAML::Node &node) : node_(node)
{
    DLOG_INFO << "[DBConfig] 开始解析 'database' 配置...";
    try
    {
        // 先通过 getter 从 YAML 节点加载数据到成员变量
        m_host = getHost();
        m_user = getUser();
        m_password = getPassword();
        m_dbName = getDBName();
        m_port = getPort();
        m_initSize = getInitSize();
        m_maxSize = getMaxSize();
        m_maxIdleTime = getMaxIdleTime();
        m_connectionTimeout = getConnectionTimeout();

        // 然后再用加载好的成员变量进行验证
        validateConfig();
        DLOG_INFO << "[DBConfig] 数据库配置校验通过";
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "[DBConfig] 配置解析或验证失败: " << e.what();
        throw; // 向上抛出异常
    }
}

std::string DBConfig::getHost() const
{
    return (node_ && node_["host"]) ? node_["host"].as<std::string>() : "";
}

std::string DBConfig::getUser() const
{
    return (node_ && node_["user"]) ? node_["user"].as<std::string>() : "root";
}

std::string DBConfig::getPassword() const
{
    return (node_ && node_["password"]) ? node_["password"].as<std::string>() : "";
}

std::string DBConfig::getDBName() const
{
    return (node_ && node_["dbname"]) ? node_["dbname"].as<std::string>() : "";
}

int DBConfig::getPort() const
{
    return (node_ && node_["port"]) ? node_["port"].as<int>() : 0;
}

unsigned int DBConfig::getInitSize() const
{
    return (node_ && node_["initSize"]) ? node_["initSize"].as<unsigned int>() : 0;
}

unsigned int DBConfig::getMaxSize() const
{
    return (node_ && node_["maxSize"]) ? node_["maxSize"].as<unsigned int>() : 0;
}

int DBConfig::getMaxIdleTime() const
{
    return (node_ && node_["maxIdleTime"]) ? node_["maxIdleTime"].as<int>() : 0;
}

int DBConfig::getConnectionTimeout() const
{
    return (node_ && node_["connectionTimeout"]) ? node_["connectionTimeout"].as<int>() : 0;
}

void DBConfig::validateConfig() const
{
    if (m_host.empty() || m_user.empty() || m_dbName.empty() || m_port <= 0 ||
        m_initSize <= 0 || m_maxSize <= 0 || m_maxIdleTime <= 0 || m_connectionTimeout <= 0)
    {
        DLOG_ERROR << "[DBConfig] 数据库配置参数无效";
        throw std::invalid_argument("Invalid or missing database config parameters.");
    }
    if (m_initSize > m_maxSize)
    {
        DLOG_ERROR << "[DBConfig] initSize 不能大于 maxSize";
        throw std::invalid_argument("initSize cannot be greater than maxSize.");
    }
}

bool DBConfig::isValid() const
{
    try
    {
        validateConfig();
        return true;
    }
    catch (const std::invalid_argument &)
    {
        return false;
    }
}