#pragma once

#include "LogLevel.h"
#include "LogEvent.h"
#include "LogFormatter.h"
#include "LogFilter.h"
#include "noncopyable.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <fstream>

/**
 * @brief 异步输出函数，供FileLogAppender调用
 * 将日志内容异步写入目标位置
 * @param msg 日志消息
 * @param len 消息长度
 */
extern void asyncOutput(const char *msg, int len);

/**
 * @brief 全局异步输出函数指针
 * 用于在测试或运行时动态替换异步输出实现
 */
extern void (*g_asyncOutputFunc)(const char *msg, int len);

/**
 * @brief 日志输出器基类
 *
 * LogAppender负责将日志事件输出到目标位置（如控制台、文件等）。
 * 每个输出器可以有自己的日志级别、格式器和过滤器。
 */
class LogAppender : noncopyable
{
public:
    using ptr = std::shared_ptr<LogAppender>;

    /**
     * @brief 虚析构函数
     */
    virtual ~LogAppender() {}

    /**
     * @brief 输出日志（纯虚函数）
     * @param logger 产生日志的日志器
     * @param event 日志事件
     */
    virtual void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) = 0;

    /**
     * @brief 设置格式器
     * @param formatter 格式器
     */
    void setFormatter(LogFormatter::ptr formatter);

    /**
     * @brief 获取格式器
     * @return 格式器
     */
    LogFormatter::ptr getFormatter() const;

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLevel(Level level) { m_level = level; }

    /**
     * @brief 获取日志级别
     * @return 日志级别
     */
    Level getLevel() const { return m_level; }

    /**
     * @brief 添加日志过滤器
     * @param filter 日志过滤器
     */
    void addFilter(LogFilter::ptr filter);

    /**
     * @brief 清除所有过滤器
     */
    void clearFilters();

protected:
    /**
     * @brief 检查是否应该过滤本条日志
     * @param event 日志事件
     * @return true表示应该过滤掉(不输出)，false表示不过滤(正常输出)
     */
    bool shouldFilter(LogEvent::ptr event) const;

protected:
    /// 日志级别
    Level m_level = Level::DEBUG;
    /// 格式器
    LogFormatter::ptr m_formatter;
    /// 互斥锁，保护多线程操作
    std::mutex m_mutex;
    /// 过滤器列表
    std::vector<LogFilter::ptr> m_filters;
};

/**
 * @brief 输出到控制台的Appender
 * 将日志输出到标准输出流（终端）
 */
class StdoutLogAppender : public LogAppender
{
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;

    /**
     * @brief 构造函数
     */
    StdoutLogAppender();

    /**
     * @brief 输出日志到控制台
     * @param logger 产生日志的日志器
     * @param event 日志事件
     */
    void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) override;
};

/**
 * @brief 输出到文件的Appender
 * 将日志输出到指定文件，支持同步和异步两种模式
 */
class FileLogAppender : public LogAppender
{
public:
    using ptr = std::shared_ptr<FileLogAppender>;

    /**
     * @brief 构造函数
     * @param filename 日志文件名
     */
    FileLogAppender(const std::string &filename);

    /**
     * @brief 析构函数，关闭文件
     */
    ~FileLogAppender();

    /**
     * @brief 输出日志到文件
     * @param logger 产生日志的日志器
     * @param event 日志事件
     */
    void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) override;

    /**
     * @brief 重新打开日志文件
     * @return 是否成功打开
     */
    bool reopen();

private:
    /// 日志文件名
    std::string m_filename;
    /// 文件输出流
    std::ofstream m_filestream;
};