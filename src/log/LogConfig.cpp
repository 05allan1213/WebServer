#include "log/LogConfig.h"
#include "log/Log.h"

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
 * 解析YAML配置文件，读取日志相关参数并进行验证
 */
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

    // 验证配置
    validateConfig(basename, rollSize, flushInterval, rollMode, fileLevel, consoleLevel);
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
 * 对每个配置参数进行严格验证，确保配置的有效性
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
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.roll_size必须大于0，当前值: " << rollSize;
        throw std::invalid_argument("log.roll_size必须大于0");
    }

    // 验证刷新间隔
    if (flushInterval <= 0)
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.flush_interval必须大于0，当前值: " << flushInterval;
        throw std::invalid_argument("log.flush_interval必须大于0");
    }

    // 验证滚动模式
    if (rollMode != "SIZE" && rollMode != "TIME" && rollMode != "SIZE_HOURLY")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.roll_mode必须是SIZE/TIME/SIZE_HOURLY之一，当前值: " << rollMode;
        throw std::invalid_argument("log.roll_mode必须是SIZE/TIME/SIZE_HOURLY之一");
    }

    // 验证日志级别
    if (fileLevel != "DEBUG" && fileLevel != "INFO" && fileLevel != "WARN" &&
        fileLevel != "ERROR" && fileLevel != "FATAL")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.file_level必须是有效的日志级别，当前值: " << fileLevel;
        throw std::invalid_argument("log.file_level必须是有效的日志级别");
    }

    if (consoleLevel != "DEBUG" && consoleLevel != "INFO" && consoleLevel != "WARN" &&
        consoleLevel != "ERROR" && consoleLevel != "FATAL")
    {
        DLOG_ERROR << "LogConfig: 配置验证失败 - log.console_level必须是有效的日志级别，当前值: " << consoleLevel;
        throw std::invalid_argument("log.console_level必须是有效的日志级别");
    }

    DLOG_INFO << "LogConfig: 配置验证通过";
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