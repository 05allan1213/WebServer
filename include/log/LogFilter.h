#pragma once

#include "LogEvent.h"
#include "noncopyable.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <regex>

/**
 * @brief 日志过滤器基类，定义过滤条件接口
 *
 * LogFilter允许基于不同条件过滤日志，如内容匹配、级别、文件名等。
 * 使用策略模式设计，可灵活扩展不同的过滤规则。
 */
class LogFilter : public noncopyable
{
public:
    using ptr = std::shared_ptr<LogFilter>;

    virtual ~LogFilter() {}

    /**
     * @brief 判断日志是否应该被过滤掉
     * @param event 日志事件
     * @return true表示不应记录此日志(过滤掉)，false表示应该记录
     */
    virtual bool filter(LogEvent::ptr event) = 0;

    /**
     * @brief 获取过滤器名称
     * @return 过滤器名称
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief 级别过滤器，根据日志级别过滤
 */
class LevelFilter : public LogFilter
{
public:
    /**
     * @brief 构造函数
     * @param level 最低允许的日志级别
     */
    LevelFilter(Level level);

    bool filter(LogEvent::ptr event) override;
    std::string getName() const override;

private:
    Level m_level;
};

/**
 * @brief 正则表达式过滤器，基于内容匹配过滤
 */
class RegexFilter : public LogFilter
{
public:
    /**
     * @brief 构造函数
     * @param pattern 正则表达式模式
     * @param exclude true表示匹配则过滤，false表示匹配则保留
     */
    RegexFilter(const std::string &pattern, bool exclude = true);

    bool filter(LogEvent::ptr event) override;
    std::string getName() const override;

private:
    std::regex m_regex; // 正则表达式
    bool m_exclude;     // true表示匹配则过滤，false表示匹配则保留
};

/**
 * @brief 文件名过滤器，基于文件名过滤
 */
class FileFilter : public LogFilter
{
public:
    /**
     * @brief 构造函数
     * @param filename 要匹配的文件名（部分匹配）
     * @param exclude true表示匹配则过滤，false表示匹配则保留
     */
    FileFilter(const std::string &filename, bool exclude = false);

    bool filter(LogEvent::ptr event) override;
    std::string getName() const override;

private:
    std::string m_filename;
    bool m_exclude;
};

/**
 * @brief 复合过滤器，组合多个过滤器
 */
class CompositeFilter : public LogFilter
{
public:
    /**
     * @brief 构造函数
     * @param all true表示所有子过滤器都通过才通过(AND关系)，
     *            false表示任一子过滤器通过就通过(OR关系)
     */
    CompositeFilter(bool all = true);

    /**
     * @brief 添加子过滤器
     * @param filter 过滤器指针
     */
    void addFilter(LogFilter::ptr filter);

    /**
     * @brief 清空所有子过滤器
     */
    void clearFilters();

    bool filter(LogEvent::ptr event) override;
    std::string getName() const override;

private:
    std::vector<LogFilter::ptr> m_filters; // 子过滤器列表
    bool m_all;                            // true表示AND关系，false表示OR关系
};

/**
 * @brief 自定义函数过滤器，使用函数对象作为过滤条件
 */
class FunctionFilter : public LogFilter
{
public:
    using FilterFunction = std::function<bool(LogEvent::ptr)>;

    /**
     * @brief 构造函数
     * @param func 过滤函数，接收LogEvent返回bool
     */
    FunctionFilter(FilterFunction func);

    bool filter(LogEvent::ptr event) override;
    std::string getName() const override;

private:
    FilterFunction m_func;
};