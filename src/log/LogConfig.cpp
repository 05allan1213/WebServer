#include "log/LogConfig.h"
#include "log/Log.h"
#include "base/BaseConfig.h"

/**
 * @brief 获取LogConfig单例实例
 * @return LogConfig的引用
 *
 * 使用局部静态变量实现线程安全的单例模式
 */
LogConfig &LogConfig::getInstance()
{
    static LogConfig instance;
    return instance;
}

/**
 * @brief 加载日志配置文件
 * @param filename 配置文件名
 *
 * 解析YAML配置文件,读取日志相关参数并进行验证
 */
void LogConfig::load(const std::string &filename)
{
    DLOG_INFO << "[LogConfig] 开始加载日志配置文件: " << filename;
    BaseConfig::getInstance().load(filename);
    DLOG_INFO << "[LogConfig] BaseConfig 加载完成，开始读取日志配置参数";
    const YAML::Node &config = BaseConfig::getInstance().getConfigNode();
    std::string basename = config["log"]["basename"] ? config["log"]["basename"].as<std::string>() : "logs/server";
    int rollSize = config["log"]["roll_size"] ? config["log"]["roll_size"].as<int>() : 1048576;
    int flushInterval = config["log"]["flush_interval"] ? config["log"]["flush_interval"].as<int>() : 1;
    std::string rollMode = config["log"]["roll_mode"] ? config["log"]["roll_mode"].as<std::string>() : "SIZE_HOURLY";
    bool enableFile = config["log"]["enable_file"] ? config["log"]["enable_file"].as<bool>() : true;
    bool enableAsync = config["log"]["enable_async"] ? config["log"]["enable_async"].as<bool>() : true;
    std::string fileLevel = config["log"]["file_level"] ? config["log"]["file_level"].as<std::string>() : "DEBUG";
    std::string consoleLevel = config["log"]["console_level"] ? config["log"]["console_level"].as<std::string>() : "WARN";
    DLOG_INFO << "[LogConfig] 读取到配置: basename=" << basename
              << ", roll_size=" << rollSize
              << ", flush_interval=" << flushInterval
              << ", roll_mode=" << rollMode
              << ", enable_file=" << enableFile
              << ", enable_async=" << enableAsync
              << ", file_level=" << fileLevel
              << ", console_level=" << consoleLevel;
    validateConfig(basename, rollSize, flushInterval, rollMode, fileLevel, consoleLevel);
    DLOG_INFO << "[LogConfig] 日志配置校验通过";
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
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["basename"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.basename 缺失，使用默认值 logs/server";
        return "logs/server";
    }
    return node["log"]["basename"].as<std::string>();
}

int LogConfig::getRollSize() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["roll_size"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.roll_size 缺失，使用默认值 1048576";
        return 1048576;
    }
    return node["log"]["roll_size"].as<int>();
}

int LogConfig::getFlushInterval() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["flush_interval"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.flush_interval 缺失，使用默认值 1";
        return 1;
    }
    return node["log"]["flush_interval"].as<int>();
}

std::string LogConfig::getRollMode() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["roll_mode"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.roll_mode 缺失，使用默认值 SIZE_HOURLY";
        return "SIZE_HOURLY";
    }
    return node["log"]["roll_mode"].as<std::string>();
}

bool LogConfig::getEnableFile() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["enable_file"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.enable_file 缺失，使用默认值 true";
        return true;
    }
    return node["log"]["enable_file"].as<bool>();
}

bool LogConfig::getEnableAsync() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["enable_async"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.enable_async 缺失，使用默认值 true";
        return true;
    }
    return node["log"]["enable_async"].as<bool>();
}

std::string LogConfig::getFileLevel() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["file_level"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.file_level 缺失，使用默认值 DEBUG";
        return "DEBUG";
    }
    return node["log"]["file_level"].as<std::string>();
}

std::string LogConfig::getConsoleLevel() const
{
    const auto &node = BaseConfig::getInstance().getConfigNode();
    if (!node["log"] || !node["log"]["console_level"])
    {
        DLOG_WARN << "[LogConfig] 配置项 log.console_level 缺失，使用默认值 WARN";
        return "WARN";
    }
    return node["log"]["console_level"].as<std::string>();
}