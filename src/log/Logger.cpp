#include "Logger.h"
#include "LogFormatter.h"
#include "LogAppender.h"
#include <iostream>
#include <algorithm>

/**
 * @brief Logger构造函数
 * @param name 日志器名称
 * @details Logger构造时，不再默认添加任何Appender，Appender的配置完全交给LogManager
 */
Logger::Logger(const std::string &name)
    : m_name(name), m_level(Level::DEBUG), m_parent(nullptr)
{
    // Logger构造时，不再默认添加任何Appender
    // Appender的配置完全交给LogManager
}

/**
 * @brief 记录日志的核心方法
 * @param level 日志级别
 * @param event 日志事件
 * @details 只有当日志事件的级别 >= 日志器设定的级别时，才进行处理
 */
void Logger::log(Level level, LogEvent::ptr event)
{
    // 只有当日志事件的级别 >= 日志器设定的级别时，才进行处理
    if (level >= m_level)
    {
        auto self = shared_from_this();
        // 检查是否被过滤器过滤
        for (auto &filter : m_filters)
        {
            if (filter->filter(event))
            {
                // 日志被过滤掉，直接返回
                return;
            }
        }

        // 遍历自己的appender，进行输出
        for (auto &appender : m_appenders)
        {
            appender->log(self, event);
        }

        // 如果允许继承且存在父logger，则将日志事件向上传递
        if (m_enableInherit && m_parent)
        {
            m_parent->log(level, event);
        }
    }
}

/**
 * @brief 记录DEBUG级别日志
 * @param event 日志事件
 */
void Logger::debug(LogEvent::ptr event)
{
    log(Level::DEBUG, event);
}

/**
 * @brief 记录INFO级别日志
 * @param event 日志事件
 */
void Logger::info(LogEvent::ptr event)
{
    log(Level::INFO, event);
}

/**
 * @brief 记录WARN级别日志
 * @param event 日志事件
 */
void Logger::warn(LogEvent::ptr event)
{
    log(Level::WARN, event);
}

/**
 * @brief 记录ERROR级别日志
 * @param event 日志事件
 */
void Logger::error(LogEvent::ptr event)
{
    log(Level::ERROR, event);
}

/**
 * @brief 记录FATAL级别日志
 * @param event 日志事件
 */
void Logger::fatal(LogEvent::ptr event)
{
    log(Level::FATAL, event);
}

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void Logger::setLevel(Level level)
{
    m_level = level;
}

/**
 * @brief 设置日志格式化器
 * @param formatter 格式化器指针
 */
void Logger::setFormatter(LogFormatter::ptr formatter)
{
    m_formatter = formatter;
}

/**
 * @brief 设置日志格式化器模板
 * @param pattern 格式化模板字符串
 */
void Logger::setFormatter(const std::string &pattern)
{
    m_formatter = std::make_shared<LogFormatter>(pattern);
}

/**
 * @brief 获取日志格式化器
 * @return 格式化器指针
 */
LogFormatter::ptr Logger::getFormatter()
{
    return m_formatter;
}

/**
 * @brief 添加日志输出器
 * @param appender 输出器指针
 */
void Logger::addAppender(LogAppenderPtr appender)
{
    m_appenders.push_back(appender);
}

/**
 * @brief 删除日志输出器
 * @param appender 输出器指针
 */
void Logger::delAppender(LogAppenderPtr appender)
{
    auto it = std::find(m_appenders.begin(), m_appenders.end(), appender);
    if (it != m_appenders.end())
    {
        m_appenders.erase(it);
    }
}

/**
 * @brief 清空所有日志输出器
 */
void Logger::clearAppenders()
{
    m_appenders.clear();
}

/**
 * @brief 添加日志过滤器
 * @param filter 过滤器指针
 */
void Logger::addFilter(LogFilter::ptr filter)
{
    m_filters.push_back(filter);
}

/**
 * @brief 清空所有日志过滤器
 */
void Logger::clearFilters()
{
    m_filters.clear();
}