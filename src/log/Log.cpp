#include "Log.h"

/**
 * @brief 初始化日志系统
 * 如果日志系统已经初始化，将重新配置它
 * @param asyncLogBasename 异步日志文件基础名，为空则不使用异步日志
 * @param asyncLogRollSize 异步日志单文件最大大小，默认10MB
 * @param asyncLogFlushInterval 异步日志刷新间隔(秒)，默认1秒
 */
void initLogSystem(const std::string &asyncLogBasename,
                   off_t asyncLogRollSize,
                   int asyncLogFlushInterval)
{
    LogManager::getInstance().init(asyncLogBasename, asyncLogRollSize, asyncLogFlushInterval);
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