#include "db/DBConfig.h"
#include <iostream>

DBConfig::DBConfig(const std::string &configPath)
{
    loadAndValidate(configPath);
}

void DBConfig::loadAndValidate(const std::string &configPath)
{
    try
    {
        YAML::Node config = YAML::LoadFile(configPath);
        auto db = config["database"];
        if (!db)
        {
            m_valid = false;
            m_errorMsg = "Missing 'database' section in config.";
            return;
        }
        m_host = db["host"] ? db["host"].as<std::string>() : "";
        m_user = db["user"] ? db["user"].as<std::string>() : "";
        m_password = db["password"] ? db["password"].as<std::string>() : "";
        m_dbName = db["dbname"] ? db["dbname"].as<std::string>() : "";
        m_port = db["port"] ? db["port"].as<int>() : 0;
        m_initSize = db["initSize"] ? db["initSize"].as<unsigned int>() : 0;
        m_maxSize = db["maxSize"] ? db["maxSize"].as<unsigned int>() : 0;
        m_maxIdleTime = db["maxIdleTime"] ? db["maxIdleTime"].as<int>() : 0;
        m_connectionTimeout = db["connectionTimeout"] ? db["connectionTimeout"].as<int>() : 0;
        // 校验
        if (m_host.empty() || m_user.empty() || m_dbName.empty() || m_port <= 0 ||
            m_initSize == 0 || m_maxSize == 0 || m_maxIdleTime <= 0 || m_connectionTimeout <= 0)
        {
            m_valid = false;
            m_errorMsg = "Invalid or missing database config parameters.";
            return;
        }
        if (m_initSize > m_maxSize)
        {
            m_valid = false;
            m_errorMsg = "initSize cannot be greater than maxSize.";
            return;
        }
        m_valid = true;
        m_errorMsg = "";
    }
    catch (const std::exception &e)
    {
        m_valid = false;
        m_errorMsg = e.what();
    }
}
