#include "Log.h"
#include "base/Config.h"

/**
 * @brief 初始化日志系统
 * 如果日志系统已经初始化，将重新配置它
 * @param asyncLogBasename 异步日志文件基础名，为空则不使用异步日志
 * @param asyncLogRollSize 异步日志单文件最大大小，默认10MB
 * @param asyncLogFlushInterval 异步日志刷新间隔(秒)，默认1秒
 * @param rollMode 日志滚动模式，默认按大小滚动
 */
void initLogSystem(const std::string &asyncLogBasename,
                   off_t asyncLogRollSize,
                   int asyncLogFlushInterval,
                   LogFile::RollMode rollMode)
{
    LogManager::getInstance().init(asyncLogBasename, asyncLogRollSize, asyncLogFlushInterval, rollMode);
}

/**
 * @brief 初始化日志系统
 * 如果日志系统已经初始化，将重新配置它
 * 自动从Config获取参数并调用有参版本
 */
void initLogSystem()
{
    Config::getInstance().load("configs/config.yml");
    const auto &config = Config::getInstance();
    bool enableFile = config.getLogEnableFile();
    std::string fileLevelStr = config.getLogFileLevel();
    std::string consoleLevelStr = config.getLogConsoleLevel();
    // 解析字符串为Level枚举，并设置到Logger/LogAppender
    std::string rollModeStr = config.getLogRollMode();
    LogFile::RollMode rollMode = LogFile::RollMode::SIZE_HOURLY;
    if (rollModeStr == "SIZE")
        rollMode = LogFile::RollMode::SIZE;
    else if (rollModeStr == "HOURLY")
        rollMode = LogFile::RollMode::HOURLY;
    else if (rollModeStr == "SIZE_HOURLY")
        rollMode = LogFile::RollMode::SIZE_HOURLY;
    initLogSystem(config.getLogBasename(), config.getLogRollSize(), config.getLogFlushInterval(), rollMode);
}

/**
 * @brief 设置日志器的级别
 * @param name 日志器名称
 * @param level 日志级别
 */
void setLoggerLevel(const std::string &name, Level level)
{
    auto logger = getLogger(name);
    if (logger)
    {
        logger->setLevel(level);
    }
}

/**
 * @brief 设置根日志器的级别
 * @param level 日志级别
 */
void setRootLoggerLevel(Level level)
{
    auto root = LogManager::getInstance().getRoot();
    if (root)
    {
        root->setLevel(level);
    }
}

/**
 * @brief 快速获取日志器的全局函数
 * @param name 日志器名称，默认为"root"
 * @return 日志器智能指针
 */
Logger::ptr getLogger(const std::string &name)
{
    return LogManager::getInstance().getLogger(name);
}

/**
 * @brief 设置日志滚动模式
 * @param mode 滚动模式
 */
void setLogRollMode(LogFile::RollMode mode)
{
    LogManager::getInstance().setRollMode(mode);
}