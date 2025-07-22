#include "db/DBConfig.h"
#include "log/Log.h"
#include <iostream>

DBConfig::DBConfig(const std::string &configPath)
{
    DLOG_INFO << "[DBConfig] 开始加载数据库配置文件: " << configPath;
    loadAndValidate(configPath);
    DLOG_INFO << "[DBConfig] 数据库配置加载并校验通过";
}

void DBConfig::loadAndValidate(const std::string &configPath)
{
    try
    {
        YAML::Node config = YAML::LoadFile(configPath);
        auto db = config["database"];
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
        // 校验
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
    catch (const std::exception &e)
    {
        DLOG_ERROR << "[DBConfig] 加载或校验数据库配置异常: " << e.what();
        throw;
    }
}
