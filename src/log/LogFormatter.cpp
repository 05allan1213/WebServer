#include "LogFormatter.h"
#include "Logger.h"
#include <functional>
#include <map>
#include <iostream>
#include <tuple>
#include <ctime>

// ----------- 以下是所有具体的 FormatItem 子类实现 -----------

// 消息内容
class MessageFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getStringStream().str();
    }
};

// 日志级别
class LevelFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << LogLevelToString(event->getLevel());
    }
};

// 文件名
class FilenameFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getFile();
    }
};

// 行号
class LineFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getLine();
    }
};

// 换行
class NewLineFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << "\n"; // 使用 \n 而不是 std::endl 来避免刷新缓冲区
    }
};

// Tab
class TabFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << "\t";
    }
};

// 线程ID
class ThreadIdFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getThreadId();
    }
};

// 日志器名称
class NameFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << logger->getName();
    }
};

// 时间
class DateTimeFormatItem : public FormatItem
{
public:
    DateTimeFormatItem(const std::string &format = "%Y-%m-%d %H:%M:%S") : m_format(format)
    {
        if (m_format.empty())
        {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_format;
};

// 普通字符串
class StringFormatItem : public FormatItem
{
public:
    StringFormatItem(const std::string &str) : m_string(str) {}
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << m_string;
    }

private:
    std::string m_string;
};

// ----------- LogFormatter 类的实现 -----------

LogFormatter::LogFormatter(const std::string &pattern) : m_pattern(pattern)
{
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogEvent::ptr event)
{
    std::stringstream ss;
    for (auto &item : m_items)
    {
        item->format(ss, logger, event);
    }
    return ss.str();
}

void LogFormatter::init()
{
    // str, format, type
    // type: 0 for string, 1 for format item
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;
    for (size_t i = 0; i < m_pattern.size(); ++i)
    {
        if (m_pattern[i] != '%')
        {
            nstr.append(1, m_pattern[i]);
            continue;
        }

        if ((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%')
        {
            nstr.append(1, '%');
            i++; // skip next '%'
            continue;
        }

        if (!nstr.empty())
        {
            vec.emplace_back(nstr, "", 0);
            nstr.clear();
        }

        size_t n = i + 1;
        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;
        while (n < m_pattern.size())
        {
            if (!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')
            {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if (fmt_status == 0)
            {
                if (m_pattern[n] == '{')
                {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // start parsing format
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            }
            else if (fmt_status == 1)
            {
                if (m_pattern[n] == '}')
                {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 2; // parsing format finished
                    ++n;
                    break;
                }
            }
            ++n;
            if (n == m_pattern.size())
            {
                if (str.empty())
                {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if (fmt_status == 0)
        {
            vec.emplace_back(str, "", 1);
            i = n - 1;
        }
        else if (fmt_status == 1)
        {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.emplace_back("<<pattern_error>>", "", 0);
        }
        else if (fmt_status == 2)
        {
            vec.emplace_back(str, fmt, 1);
            i = n - 1;
        }
    }

    if (!nstr.empty())
    {
        vec.emplace_back(nstr, "", 0);
    }

    // 使用 static map 实现工厂模式
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> s_format_items = {
#define XX(str, C) {str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }}
#define XX_SIMPLE(str, C) {str, [](const std::string &fmt) { return FormatItem::ptr(new C()); }}

        XX("d", DateTimeFormatItem),
        XX_SIMPLE("m", MessageFormatItem),
        XX_SIMPLE("p", LevelFormatItem),
        XX_SIMPLE("c", NameFormatItem),
        XX_SIMPLE("t", ThreadIdFormatItem),
        XX_SIMPLE("n", NewLineFormatItem),
        XX_SIMPLE("f", FilenameFormatItem),
        XX_SIMPLE("l", LineFormatItem),
        XX_SIMPLE("T", TabFormatItem)
#undef XX
#undef XX_SIMPLE
    };

    for (auto &i : vec)
    {
        if (std::get<2>(i) == 0)
        {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        }
        else
        {
            auto it = s_format_items.find(std::get<0>(i));
            if (it == s_format_items.end())
            {
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            }
            else
            {
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }
    }
}