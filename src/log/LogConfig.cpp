#include "log/LogConfig.h"
#include "log/Log.h"

LogConfig &LogConfig::getInstance()
{
    static LogConfig instance;
    return instance;
}

void LogConfig::load(const std::string &filename)
{
    DLOG_INFO << "LogConfig: 开始加载配置文件 " << filename;
    config_ = YAML::LoadFile(filename);
    DLOG_INFO << "LogConfig: 配置文件加载完成";

    // 读取并记录配置值
    std::string basename = config_["log"]["basename"].as<std::string>();
    int rollSize = config_["log"]["roll_size"].as<int>();
    int flushInterval = config_["log"]["flush_interval"].as<int>();
    std::string rollMode = config_["log"]["roll_mode"].as<std::string>();
    bool enableFile = config_["log"]["enable_file"].as<bool>();
    std::string fileLevel = config_["log"]["file_level"].as<std::string>();
    std::string consoleLevel = config_["log"]["console_level"].as<std::string>();

    DLOG_INFO << "LogConfig: 读取到配置 - basename=" << basename
              << ", roll_size=" << rollSize
              << ", flush_interval=" << flushInterval
              << ", roll_mode=" << rollMode
              << ", enable_file=" << enableFile
              << ", file_level=" << fileLevel
              << ", console_level=" << consoleLevel;
}

std::string LogConfig::getBasename() const
{
    return config_["log"]["basename"].as<std::string>();
}

int LogConfig::getRollSize() const
{
    return config_["log"]["roll_size"].as<int>();
}

int LogConfig::getFlushInterval() const
{
    return config_["log"]["flush_interval"].as<int>();
}

std::string LogConfig::getRollMode() const
{
    return config_["log"]["roll_mode"].as<std::string>();
}

bool LogConfig::getEnableFile() const
{
    return config_["log"]["enable_file"].as<bool>();
}

std::string LogConfig::getFileLevel() const
{
    return config_["log"]["file_level"].as<std::string>();
}

std::string LogConfig::getConsoleLevel() const
{
    return config_["log"]["console_level"].as<std::string>();
}