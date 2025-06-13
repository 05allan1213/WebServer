#include "Logger.h"
#include "LogFormatter.h"
#include "LogAppender.h"
#include <iostream>
#include <algorithm>

Logger::Logger(const std::string &name)
    : m_name(name), m_level(Level::DEBUG)
{
    // Logger 构造时，默认添加一个输出到控制台的 Appender
    LogAppenderPtr default_appender(new StdoutLogAppender());
    // 给这个默认的 appender 设置一个默认的 formatter
    default_appender->setFormatter(std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
    m_appenders.push_back(default_appender);
}

void Logger::log(Level level, LogEvent::ptr event)
{
    // 只有当日志事件的级别 >= 日志器设定的级别时，才进行输出
    if (level >= m_level)
    {
        // 检查是否被过滤器过滤
        for (auto &filter : m_filters)
        {
            if (filter->filter(event))
            {
                // 日志被过滤掉，直接返回
                return;
            }
        }

        // 使用 formatter 格式化日志
        auto self = shared_from_this();
        // 遍历所有 appender，调用它们的 log 方法
        for (auto &appender : m_appenders)
        {
            appender->log(self, event);
        }
    }
}

// 其他级别的函数，本质上都是调用 log()
void Logger::debug(LogEvent::ptr event)
{
    log(Level::DEBUG, event);
}

void Logger::info(LogEvent::ptr event)
{
    log(Level::INFO, event);
}

void Logger::warn(LogEvent::ptr event)
{
    log(Level::WARN, event);
}

void Logger::error(LogEvent::ptr event)
{
    log(Level::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event)
{
    log(Level::FATAL, event);
}

void Logger::setLevel(Level level)
{
    m_level = level;
}

void Logger::setFormatter(LogFormatter::ptr formatter)
{
    m_formatter = formatter;
}

void Logger::setFormatter(const std::string &pattern)
{
    m_formatter = std::make_shared<LogFormatter>(pattern);
}

LogFormatter::ptr Logger::getFormatter()
{
    return m_formatter;
}

void Logger::addAppender(LogAppenderPtr appender)
{
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppenderPtr appender)
{
    auto it = std::find(m_appenders.begin(), m_appenders.end(), appender);
    if (it != m_appenders.end())
    {
        m_appenders.erase(it);
    }
}

void Logger::clearAppenders()
{
    m_appenders.clear();
}

void Logger::addFilter(LogFilter::ptr filter)
{
    m_filters.push_back(filter);
}

void Logger::clearFilters()
{
    m_filters.clear();
}