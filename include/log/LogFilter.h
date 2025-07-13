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
    LevelFilter(Level level) : m_level(level) {}

    bool filter(LogEvent::ptr event) override
    {
        return event->getLevel() < m_level;
    }

    std::string getName() const override
    {
        return "LevelFilter";
    }

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
    RegexFilter(const std::string &pattern, bool exclude = true)
        : m_regex(pattern), m_exclude(exclude) {}

    bool filter(LogEvent::ptr event) override
    {
        std::string content = event->getStringStream().str();
        bool match = std::regex_search(content, m_regex);
        // 如果m_exclude为true，匹配则过滤；否则匹配则保留
        return m_exclude ? match : !match;
    }

    std::string getName() const override
    {
        return "RegexFilter";
    }

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
    FileFilter(const std::string &filename, bool exclude = false)
        : m_filename(filename), m_exclude(exclude) {}

    bool filter(LogEvent::ptr event) override
    {
        std::string filename = event->getFile();
        bool match = filename.find(m_filename) != std::string::npos;
        return match == m_exclude;
    }

    std::string getName() const override
    {
        return "FileFilter";
    }

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
    CompositeFilter(bool all = true) : m_all(all) {}

    /**
     * @brief 添加子过滤器
     * @param filter 过滤器指针
     */
    void addFilter(LogFilter::ptr filter)
    {
        m_filters.push_back(filter);
    }

    /**
     * @brief 清空所有子过滤器
     */
    void clearFilters()
    {
        m_filters.clear();
    }

    bool filter(LogEvent::ptr event) override
    {
        // 没有子过滤器时不过滤
        if (m_filters.empty())
        {
            return false;
        }

        // 根据m_all决定使用AND还是OR逻辑
        for (auto &filter : m_filters)
        {
            bool result = filter->filter(event);
            // AND逻辑：任一子过滤器返回true(过滤)，则整体返回true(过滤)
            // OR逻辑：任一子过滤器返回false(不过滤)，则整体返回false(不过滤)
            if (m_all ? result : !result)
            {
                return m_all;
            }
        }
        // AND逻辑：所有子过滤器都返回false(不过滤)，则整体返回false(不过滤)
        // OR逻辑：所有子过滤器都返回true(过滤)，则整体返回true(过滤)
        return !m_all;
    }

    std::string getName() const override
    {
        return "CompositeFilter";
    }

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
    FunctionFilter(FilterFunction func) : m_func(func) {}

    bool filter(LogEvent::ptr event) override
    {
        return m_func(event);
    }

    std::string getName() const override
    {
        return "FunctionFilter";
    }

private:
    FilterFunction m_func;
};