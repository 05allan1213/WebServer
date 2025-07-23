#include "db/DBConfig.h"
#include "log/Log.h"
#include <iostream>

// 单例实现
DBConfig &DBConfig::getInstance()
{
    static DBConfig instance;
    return instance;
}

// load 实现，读取配置并校验
void DBConfig::load(const std::string &filename)
{
    DLOG_INFO << "[DBConfig] 开始加载数据库配置文件: " << filename;
    config_ = YAML::LoadFile(filename);
    const auto &db = config_["database"];
    if (!db)
    {
        DLOG_ERROR << "[DBConfig] 配置文件缺少 database 节，使用默认配置";
        throw std::invalid_argument("Missing 'database' section in config.");
    }
    m_host = db["host"] ? db["host"].as<std::string>() : "127.0.0.1";
    if (!db["host"])
        DLOG_WARN << "[DBConfig] 配置项 database.host 缺失，使用默认值 127.0.0.1";
    m_user = db["user"] ? db["user"].as<std::string>() : "root";
    if (!db["user"])
        DLOG_WARN << "[DBConfig] 配置项 database.user 缺失，使用默认值 root";
    m_password = db["password"] ? db["password"].as<std::string>() : "";
    if (!db["password"])
        DLOG_WARN << "[DBConfig] 配置项 database.password 缺失，使用默认值 空";
    m_dbName = db["dbname"] ? db["dbname"].as<std::string>() : "test";
    if (!db["dbname"])
        DLOG_WARN << "[DBConfig] 配置项 database.dbname 缺失，使用默认值 test";
    m_port = db["port"] ? db["port"].as<int>() : 3306;
    if (!db["port"])
        DLOG_WARN << "[DBConfig] 配置项 database.port 缺失，使用默认值 3306";
    m_initSize = db["initSize"] ? db["initSize"].as<unsigned int>() : 5;
    if (!db["initSize"])
        DLOG_WARN << "[DBConfig] 配置项 database.initSize 缺失，使用默认值 5";
    m_maxSize = db["maxSize"] ? db["maxSize"].as<unsigned int>() : 20;
    if (!db["maxSize"])
        DLOG_WARN << "[DBConfig] 配置项 database.maxSize 缺失，使用默认值 20";
    m_maxIdleTime = db["maxIdleTime"] ? db["maxIdleTime"].as<int>() : 60;
    if (!db["maxIdleTime"])
        DLOG_WARN << "[DBConfig] 配置项 database.maxIdleTime 缺失，使用默认值 60";
    m_connectionTimeout = db["connectionTimeout"] ? db["connectionTimeout"].as<int>() : 1000;
    if (!db["connectionTimeout"])
        DLOG_WARN << "[DBConfig] 配置项 database.connectionTimeout 缺失，使用默认值 1000";
    validateConfig();
    DLOG_INFO << "[DBConfig] 数据库配置加载并校验通过";
}

// 参数校验
void DBConfig::validateConfig()
{
    if (m_host.empty() || m_user.empty() || m_dbName.empty() || m_port <= 0 ||
        m_initSize == 0 || m_maxSize == 0 || m_maxIdleTime <= 0 || m_connectionTimeout <= 0)
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
    bool valid = true;
    if (m_host.empty())
    {
        DLOG_ERROR << "[DBConfig] host 为空";
        valid = false;
    }
    if (m_user.empty())
    {
        DLOG_ERROR << "[DBConfig] user 为空";
        valid = false;
    }
    if (m_dbName.empty())
    {
        DLOG_ERROR << "[DBConfig] dbName 为空";
        valid = false;
    }
    if (m_port <= 0)
    {
        DLOG_ERROR << "[DBConfig] port 非法: " << m_port;
        valid = false;
    }
    if (m_initSize == 0)
    {
        DLOG_ERROR << "[DBConfig] initSize 不能为0";
        valid = false;
    }
    if (m_maxSize == 0)
    {
        DLOG_ERROR << "[DBConfig] maxSize 不能为0";
        valid = false;
    }
    if (m_maxIdleTime <= 0)
    {
        DLOG_ERROR << "[DBConfig] maxIdleTime 非法: " << m_maxIdleTime;
        valid = false;
    }
    if (m_connectionTimeout <= 0)
    {
        DLOG_ERROR << "[DBConfig] connectionTimeout 非法: " << m_connectionTimeout;
        valid = false;
    }
    if (m_initSize > m_maxSize)
    {
        DLOG_ERROR << "[DBConfig] initSize(" << m_initSize << ") 不能大于 maxSize(" << m_maxSize << ")";
        valid = false;
    }
    if (valid)
    {
        DLOG_INFO << "[DBConfig] 配置校验通过";
    }
    else
    {
        DLOG_ERROR << "[DBConfig] 配置校验未通过";
    }
    return valid;
}
