#pragma once

#include "LogEvent.h"
#include "LogLevel.h"
#include "LogFormatter.h"
#include "LogFilter.h"
#include "noncopyable.h"
#include <string>
#include <vector>
#include <memory>

// 前向声明
class LogAppender;
// 为了能在 Logger 中使用 LogAppender::ptr，我们需要前向声明 LogAppender 的 ptr 类型
namespace std
{
    template <typename T>
    class shared_ptr;
}
typedef std::shared_ptr<LogAppender> LogAppenderPtr;

/**
 * @brief 日志器类，作为日志系统的核心入口
 *
 * Logger是整个日志系统的核心协调者，它负责：
 * 1. 接收来自用户的日志请求
 * 2. 根据日志级别判断是否需要记录
 * 3. 将日志事件分发给各个Appender进行输出
 *
 * 每个Logger持有多个LogAppender，可以同时将日志输出到不同目标(如控制台、文件等)
 */
class Logger : public std::enable_shared_from_this<Logger>, public noncopyable
{
public:
    // 使用智能指针管理Logger对象
    using ptr = std::shared_ptr<Logger>;

public:
    /**
     * @brief 构造函数
     * @param name 日志器名称，默认为"root"
     */
    Logger(const std::string &name = "root");

    /**
     * @brief 记录日志的核心方法
     * @param level 日志级别
     * @param event 日志事件
     */
    void log(Level level, LogEvent::ptr event);

    /**
     * @brief 记录DEBUG级别日志
     * @param event 日志事件
     */
    void debug(LogEvent::ptr event);

    /**
     * @brief 记录INFO级别日志
     * @param event 日志事件
     */
    void info(LogEvent::ptr event);

    /**
     * @brief 记录WARN级别日志
     * @param event 日志事件
     */
    void warn(LogEvent::ptr event);

    /**
     * @brief 记录ERROR级别日志
     * @param event 日志事件
     */
    void error(LogEvent::ptr event);

    /**
     * @brief 记录FATAL级别日志
     * @param event 日志事件
     */
    void fatal(LogEvent::ptr event);

    /**
     * @brief 获取日志器名称
     * @return 日志器名称
     */
    const std::string &getName() const { return m_name; }

    /**
     * @brief 获取日志级别
     * @return 日志级别
     */
    Level getLevel() const { return m_level; }

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLevel(Level level);

    /**
     * @brief 设置日志格式化器
     * @param formatter 格式化器指针
     */
    void setFormatter(LogFormatter::ptr formatter);

    /**
     * @brief 设置日志格式化器模板
     * @param pattern 格式化模板字符串
     */
    void setFormatter(const std::string &pattern);

    /**
     * @brief 获取日志格式化器
     * @return 格式化器指针
     */
    LogFormatter::ptr getFormatter();

    /**
     * @brief 添加日志输出器
     * @param appender 输出器指针
     */
    void addAppender(LogAppenderPtr appender);

    /**
     * @brief 删除日志输出器
     * @param appender 输出器指针
     */
    void delAppender(LogAppenderPtr appender);

    /**
     * @brief 清空所有日志输出器
     */
    void clearAppenders();

    /**
     * @brief 获取当前日志器的所有输出器
     * @return 日志输出器向量的常引用
     */
    const std::vector<LogAppenderPtr> &getAppenders() const { return m_appenders; }

    /**
     * @brief 添加日志过滤器
     * @param filter 过滤器指针
     */
    void addFilter(LogFilter::ptr filter);

    /**
     * @brief 清空所有日志过滤器
     */
    void clearFilters();

private:
    std::string m_name;                      // 日志名称
    Level m_level;                           // 日志器允许输出的最低级别
    LogFormatter::ptr m_formatter;           // 日志格式化器
    std::vector<LogAppenderPtr> m_appenders; // 日志输出器集合
    std::vector<LogFilter::ptr> m_filters;   // 日志过滤器集合
};

/**
 * @brief 基础日志宏，根据级别判断是否创建LogEvent并记录日志
 *
 * 宏的工作流程:
 * 1. 判断logger的级别是否允许输出指定级别的日志
 * 2. 如果允许，创建一个临时的LogEventWrap对象
 * 3. 该对象在生命周期结束时(语句末尾)的析构函数会自动提交日志
 * 4. 允许使用流式语法(<<)向日志添加内容
 */
#define LOG_LEVEL(logger, level)     \
    if (logger->getLevel() <= level) \
    LogEventWrap(logger, std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 1, time(0), level)).getStringStream()

// 不同级别的日志宏
#define LOG_DEBUG(logger) LOG_LEVEL(logger, Level::DEBUG)
#define LOG_INFO(logger) LOG_LEVEL(logger, Level::INFO)
#define LOG_WARN(logger) LOG_LEVEL(logger, Level::WARN)
#define LOG_ERROR(logger) LOG_LEVEL(logger, Level::ERROR)
#define LOG_FATAL(logger) LOG_LEVEL(logger, Level::FATAL)