#include "Log.h"
#include "log/LogConfig.h"
#include "base/ConfigManager.h"

/**
 * @brief 初始化一个最小化的默认日志系统 (仅输出到控制台)
 * @details 用于在配置文件加载前捕获关键错误日志。
 * 只设置 root logger，级别为 DEBUG，输出到控制台。
 */
void initDefaultLogger()
{
    auto logManager = LogManager::getInstance();
    // 只有在日志系统完全未初始化时才进行设置
    if (logManager->isInitialized())
    {
        return;
    }

    auto rootLogger = logManager->getRoot();

    rootLogger->clearAppenders();

    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(
        std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
    consoleAppender->setLevel(Level::DEBUG);
    rootLogger->addAppender(consoleAppender);

    rootLogger->setLevel(Level::DEBUG);

    // 这是一个技巧：我们暂时将系统标记为已初始化，以便 DLOG_* 宏可以工作。
    // 稍后 initLogSystem() 会根据配置文件再次设置它。
    logManager->setInitialized(true);
}

/**
 * @brief 根据配置初始化日志系统
 * @details 从 ConfigManager 获取配置并初始化 LogManager。
 * 此函数应在 ConfigManager::load() 之后调用。
 */
void initLogSystem()
{
    auto logConfig = ConfigManager::getInstance().getLogConfig();
    if (!logConfig)
    {
        DLOG_ERROR << "[Log] LogConfig 未加载, 日志系统将继续使用默认的控制台输出";
        return;
    }

    std::string rollModeStr = logConfig->getRollMode();
    LogFile::RollMode rollMode = LogFile::RollMode::SIZE_HOURLY;
    if (rollModeStr == "SIZE")
        rollMode = LogFile::RollMode::SIZE;
    else if (rollModeStr == "DAILY")
        rollMode = LogFile::RollMode::DAILY;
    else if (rollModeStr == "HOURLY")
        rollMode = LogFile::RollMode::HOURLY;
    else if (rollModeStr == "MINUTELY")
        rollMode = LogFile::RollMode::MINUTELY;
    else if (rollModeStr == "SIZE_DAILY")
        rollMode = LogFile::RollMode::SIZE_DAILY;
    else if (rollModeStr == "SIZE_HOURLY")
        rollMode = LogFile::RollMode::SIZE_HOURLY;
    else if (rollModeStr == "SIZE_MINUTELY")
        rollMode = LogFile::RollMode::SIZE_MINUTELY;

    LogManager::getInstance()->init(logConfig->getBasename(), logConfig->getRollSize(), logConfig->getFlushInterval(), rollMode);
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
    auto root = LogManager::getInstance()->getRoot();
    if (root)
    {
        root->setLevel(level);
    }
}

/**
 * @brief 快速获取日志器的全局函数
 * @param name 日志器名称,默认为"root"
 * @return 日志器智能指针
 */
Logger::ptr getLogger(const std::string &name)
{
    return LogManager::getInstance()->getLogger(name);
}

/**
 * @brief 设置日志滚动模式
 * @param mode 滚动模式
 */
void setLogRollMode(LogFile::RollMode mode)
{
    LogManager::getInstance()->setRollMode(mode);
}