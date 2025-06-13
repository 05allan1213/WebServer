#pragma once

#include "LogEvent.h"
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
class LogFilter
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
        : m_pattern(pattern), m_regex(pattern), m_exclude(exclude) {}

    bool filter(LogEvent::ptr event) override
    {
        std::string content = event->getStringStream().str();
        bool match = std::regex_search(content, m_regex);
        return match == m_exclude; // 如果m_exclude为true，匹配则过滤；否则匹配则保留
    }

    std::string getName() const override
    {
        return "RegexFilter";
    }

private:
    std::string m_pattern;
    std::regex m_regex;
    bool m_exclude;
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
     * @brief 默认构造函数
     * @param all_match true表示需要全部过滤器都通过，false表示任一过滤器通过即可
     */
    CompositeFilter(bool all_match = false) : m_all_match(all_match) {}

    /**
     * @brief 添加子过滤器
     * @param filter 要添加的过滤器
     */
    void addFilter(LogFilter::ptr filter)
    {
        m_filters.push_back(filter);
    }

    bool filter(LogEvent::ptr event) override
    {
        if (m_filters.empty())
        {
            return false; // 没有过滤器则不过滤
        }

        if (m_all_match)
        {
            // 全部匹配模式：所有过滤器都返回true才过滤
            for (auto &f : m_filters)
            {
                if (!f->filter(event))
                {
                    return false;
                }
            }
            return true;
        }
        else
        {
            // 任一匹配模式：任一过滤器返回true就过滤
            for (auto &f : m_filters)
            {
                if (f->filter(event))
                {
                    return true;
                }
            }
            return false;
        }
    }

    std::string getName() const override
    {
        return "CompositeFilter";
    }

private:
    std::vector<LogFilter::ptr> m_filters;
    bool m_all_match; // true为AND关系，false为OR关系
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