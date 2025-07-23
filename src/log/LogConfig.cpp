#include "log/LogConfig.h"
#include "log/Log.h"

LogConfig::LogConfig(const YAML::Node &node) : node_(node)
{
    DLOG_INFO << "[LogConfig] 开始解析 'log' 配置...";
    try
    {
        std::string basename = getBasename();
        int rollSize = getRollSize();
        int flushInterval = getFlushInterval();
        std::string rollMode = getRollMode();
        std::string fileLevel = getFileLevel();
        std::string consoleLevel = getConsoleLevel();
        validateConfig(basename, rollSize, flushInterval, rollMode, fileLevel, consoleLevel);
        DLOG_INFO << "[LogConfig] 日志配置校验通过";
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "[LogConfig] 配置解析或验证失败: " << e.what();
        throw;
    }
}

/**
 * @brief 验证日志配置参数
 * @param basename 日志文件基础名称
 * @param rollSize 滚动大小
 * @param flushInterval 刷新间隔
 * @param rollMode 滚动模式
 * @param fileLevel 文件日志级别
 * @param consoleLevel 控制台日志级别
 *
 * 对每个配置参数进行严格验证,确保配置的有效性
 */
void LogConfig::validateConfig(const std::string &basename, int rollSize, int flushInterval,
                               const std::string &rollMode, const std::string &fileLevel,
                               const std::string &consoleLevel)
{
    DLOG_INFO << "LogConfig: 开始验证配置...";

    // 验证基础名称
    if (basename.empty())
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.basename不能为空";
        throw std::invalid_argument("log.basename不能为空");
    }

    // 验证滚动大小
    if (rollSize <= 0)
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.roll_size必须大于0,当前值: " << rollSize;
        throw std::invalid_argument("log.roll_size必须大于0");
    }

    // 验证刷新间隔
    if (flushInterval <= 0)
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.flush_interval必须大于0,当前值: " << flushInterval;
        throw std::invalid_argument("log.flush_interval必须大于0");
    }

    // 验证滚动模式
    if (rollMode != "SIZE" && rollMode != "TIME" && rollMode != "SIZE_HOURLY")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.roll_mode必须是SIZE/TIME/SIZE_HOURLY之一,当前值: " << rollMode;
        throw std::invalid_argument("log.roll_mode必须是SIZE/TIME/SIZE_HOURLY之一");
    }

    // 验证日志级别
    if (fileLevel != "DEBUG" && fileLevel != "INFO" && fileLevel != "WARN" &&
        fileLevel != "ERROR" && fileLevel != "FATAL")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.file_level必须是有效的日志级别,当前值: " << fileLevel;
        throw std::invalid_argument("log.file_level必须是有效的日志级别");
    }

    if (consoleLevel != "DEBUG" && consoleLevel != "INFO" && consoleLevel != "WARN" &&
        consoleLevel != "ERROR" && consoleLevel != "FATAL")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.console_level必须是有效的日志级别,当前值: " << consoleLevel;
        throw std::invalid_argument("log.console_level必须是有效的日志级别");
    }

    DLOG_INFO << "LogConfig: 配置验证通过";
}

std::string LogConfig::getBasename() const
{
    if (node_ && node_["basename"])
    {
        return node_["basename"].as<std::string>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.basename 缺失，使用默认值 logs/server";
    return "logs/server";
}

int LogConfig::getRollSize() const
{
    if (node_ && node_["roll_size"])
    {
        return node_["roll_size"].as<int>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.roll_size 缺失，使用默认值 1048576";
    return 1048576;
}

int LogConfig::getFlushInterval() const
{
    if (node_ && node_["flush_interval"])
    {
        return node_["flush_interval"].as<int>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.flush_interval 缺失，使用默认值 1";
    return 1;
}

std::string LogConfig::getRollMode() const
{
    if (node_ && node_["roll_mode"])
    {
        return node_["roll_mode"].as<std::string>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.roll_mode 缺失，使用默认值 SIZE_HOURLY";
    return "SIZE_HOURLY";
}

bool LogConfig::getEnableFile() const
{
    if (node_ && node_["enable_file"])
    {
        return node_["enable_file"].as<bool>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.enable_file 缺失，使用默认值 true";
    return true;
}

bool LogConfig::getEnableAsync() const
{
    if (node_ && node_["enable_async"])
    {
        return node_["enable_async"].as<bool>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.enable_async 缺失，使用默认值 true";
    return true;
}

std::string LogConfig::getFileLevel() const
{
    if (node_ && node_["file_level"])
    {
        return node_["file_level"].as<std::string>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.file_level 缺失，使用默认值 DEBUG";
    return "DEBUG";
}

std::string LogConfig::getConsoleLevel() const
{
    if (node_ && node_["console_level"])
    {
        return node_["console_level"].as<std::string>();
    }
    DLOG_WARN << "[LogConfig] 配置项 log.console_level 缺失，使用默认值 WARN";
    return "WARN";
}