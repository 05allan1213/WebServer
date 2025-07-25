#pragma once

/**
 * @file Log.h
 * @brief 日志系统统一入口头文件
 *
 * 这个头文件是日志系统的唯一入口点,包含了使用日志系统所需的所有接口。
 * 用户只需包含这一个头文件,就可以使用日志系统的全部功能。
 */

// 包含所有必要的头文件
#include "LogManager.h"
#include "Logger.h"
#include "LogLevel.h"
#include "LogEvent.h"
#include "LogEventWrap.h"
#include "LogAppender.h"
#include "LogFilter.h"
#include "base/BaseConfig.h"

/**
 * @brief 初始化日志系统
 * 如果日志系统已经初始化,将重新配置它
 * @param asyncLogBasename 异步日志文件基础名,为空则不使用异步日志
 * @param asyncLogRollSize 异步日志单文件最大大小,默认10MB
 * @param asyncLogFlushInterval 异步日志刷新间隔(秒),默认1秒
 * @param rollMode 日志滚动模式,默认按大小滚动
 */
void initLogSystem(const std::string &asyncLogBasename, size_t asyncLogRollSize, uint32_t asyncLogFlushInterval, LogFile::RollMode rollMode);

/**
 * @brief 初始化一个最小化的默认日志系统 (仅输出到控制台)
 * @details 用于在配置文件加载前捕获关键错误日志。
 */
void initDefaultLogger();

/**
 * @brief 根据配置初始化日志系统 (会覆盖默认设置)
 */
void initLogSystem();

/**
 * @brief 设置日志器的级别
 * @param name 日志器名称
 * @param level 日志级别
 */
void setLoggerLevel(const std::string &name, Level level);

/**
 * @brief 设置根日志器的级别
 * @param level 日志级别
 */
void setRootLoggerLevel(Level level);

/**
 * @brief 快速获取日志器的全局函数
 * @param name 日志器名称,默认为"root"
 * @return 日志器智能指针
 */
Logger::ptr getLogger(const std::string &name = "root");

/**
 * @brief 设置日志滚动模式
 * @param mode 滚动模式
 */
void setLogRollMode(LogFile::RollMode mode);

// 重新导出所有日志宏,确保它们可用
#ifndef LOG_LEVEL
#define LOG_LEVEL(logger, level)     \
    if (logger->getLevel() <= level) \
    LogEventWrap(logger, std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 1, time(0), level)).getStringStream()
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(logger) LOG_LEVEL(logger, Level::DEBUG)
#endif

#ifndef LOG_INFO
#define LOG_INFO(logger) LOG_LEVEL(logger, Level::INFO)
#endif

#ifndef LOG_WARN
#define LOG_WARN(logger) LOG_LEVEL(logger, Level::WARN)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(logger) LOG_LEVEL(logger, Level::ERROR)
#endif

#ifndef LOG_FATAL
#define LOG_FATAL(logger) LOG_LEVEL(logger, Level::FATAL)
#endif

// 为了方便使用,提供默认日志器的简化宏
#ifndef DLOG_DEBUG
#define DLOG_DEBUG LOG_DEBUG(getLogger())
#endif

#ifndef DLOG_INFO
#define DLOG_INFO LOG_INFO(getLogger())
#endif

#ifndef DLOG_WARN
#define DLOG_WARN LOG_WARN(getLogger())
#endif

#ifndef DLOG_ERROR
#define DLOG_ERROR LOG_ERROR(getLogger())
#endif

#ifndef DLOG_FATAL
#define DLOG_FATAL LOG_FATAL(getLogger())
#endif