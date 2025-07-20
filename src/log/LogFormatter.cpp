#include "LogFormatter.h"
#include "Logger.h"
#include <functional>
#include <map>
#include <iostream>
#include <tuple>
#include <ctime>

// ----------- 以下是所有具体的 FormatItem 子类实现 -----------

/**
 * @brief 格式化项：输出日志消息内容 (%m)
 * @details 从LogEvent中提取用户通过流式接口写入的日志正文并输出。
 */
class MessageFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getStringStream().str();
    }
};

/**
 * @brief 格式化项：输出日志级别 (%p)
 * @details 将LogEvent中的Level枚举转换为字符串表示并输出。
 */
class LevelFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << LogLevelToString(event->getLevel());
    }
};

/**
 * @brief 格式化项：输出源文件名 (%f)
 * @details 从LogEvent中提取日志记录点的源文件名并输出。
 */
class FilenameFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getFile();
    }
};

/**
 * @brief 格式化项：输出行号 (%l)
 * @details 从LogEvent中提取日志记录点的代码行号并输出。
 */
class LineFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getLine();
    }
};

/**
 * @brief 格式化项：输出换行符 (%n)
 */
class NewLineFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << "\n"; // 使用 \n 而不是 std::endl 来避免不必要的缓冲区刷新
    }
};

/**
 * @brief 格式化项：输出制表符 (%T)
 */
class TabFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << "\t";
    }
};

/**
 * @brief 格式化项：输出线程ID (%t)
 * @details 从LogEvent中提取线程ID并输出。
 */
class ThreadIdFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getThreadId();
    }
};

/**
 * @brief 格式化项：输出日志器名称 (%c)
 * @details 从传入的Logger对象中获取其名称并输出。
 */
class NameFormatItem : public FormatItem
{
public:
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        os << event->getLoggerName();
    }
};

/**
 * @brief 格式化项：输出日期和时间 (%d)
 * @details
 * - 支持通过花括号传递 strftime 兼容的格式字符串,例如 `%d{%Y-%m-%d %H:%M:%S}`。
 * - 如果没有提供格式(例如,仅使用 `%d`),则会使用默认格式 `"%Y-%m-%d %H:%M:%S"`。
 * - 如果提供了空的格式(例如 `%d{}`),也会使用默认格式。
 */
class DateTimeFormatItem : public FormatItem
{
public:
    /**
     * @brief 构造函数,接收时间格式字符串
     * @param format strftime 兼容的格式,默认为 "%Y-%m-%d %H:%M:%S"
     */
    DateTimeFormatItem(const std::string &format = "%Y-%m-%d %H:%M:%S") : m_format(format)
    {
        // 如果用户提供了空的格式字符串(例如 %d{}),则使用默认格式
        if (m_format.empty())
        {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    /**
     * @brief 格式化函数,将 LogEvent 中的时间戳格式化为字符串
     */
    void format(std::ostream &os, Logger::ptr logger, LogEvent::ptr event) override
    {
        struct tm tm;
        // 从 LogEvent 中获取事件发生的时间戳 (time_t)
        time_t time = event->getTime();
        // 使用 localtime_r 将 time_t 转换为 struct tm
        localtime_r(&time, &tm);
        char buf[64];
        // 使用 strftime 函数,根据成员变量 m_format 中存储的格式,将 struct tm 格式化为可读的时间字符串,并存入缓冲区 buf
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        // 将格式化后的时间字符串输出到流中
        os << buf;
    }

private:
    std::string m_format; // strftime兼容的时间格式字符串
};

/**
 * @brief 格式化项：输出普通字符串
 * @details 用于处理格式化模板中非格式占位符的普通文本部分。
 */
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

/**
 * @brief 核心格式化函数
 * @details 遍历预先解析好的FormatItem列表,依次调用每个项的format方法,
 *          将格式化结果拼接到一个stringstream中,最终返回完整的日志字符串。
 */
std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogEvent::ptr event)
{
    std::stringstream ss;
    for (auto &item : m_items)
    {
        item->format(ss, logger, event);
    }
    return ss.str();
}

/**
 * @brief 解析格式化模板字符串
 * @details 这是LogFormatter的核心初始化逻辑。它会解析构造时传入的pattern字符串,
 *          将其分解为一系列的FormatItem,并存储在m_items列表中,供format()方法使用。
 *          该方法使用了状态机来解析复杂的格式说明符,如带参数的%d{...}。
 */
void LogFormatter::init()
{
    // 用于存储解析结果的元组向量。每个元组包含：
    // 1. string: 格式化项的标识符或普通字符串内容
    // 2. string: 格式化项的参数(如 %d 后花括号里的内容)
    // 3. int: 类型(0表示普通字符串,1表示格式化项)
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr; // 用于临时存储普通字符串
    for (size_t i = 0; i < m_pattern.size(); ++i)
    {
        // 如果不是格式化占位符'%',则认为是普通字符串
        if (m_pattern[i] != '%')
        {
            nstr.append(1, m_pattern[i]);
            continue;
        }

        // 处理连续两个'%'的情况,即'%%',将其视为一个普通的'%'字符
        if ((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%')
        {
            nstr.append(1, '%');
            i++; // 跳过下一个'%'
            continue;
        }

        // 遇到'%',说明之前的普通字符串部分已经结束,将其存入vec
        if (!nstr.empty())
        {
            vec.emplace_back(nstr, "", 0);
            nstr.clear();
        }

        // 开始解析格式化项
        size_t n = i + 1;
        int fmt_status = 0;   // 解析状态机：0-初始,1-解析参数中,2-参数解析完毕
        size_t fmt_begin = 0; // 参数起始位置

        std::string str; // 存储格式项标识符,如'd', 'p', 'm'
        std::string fmt; // 存储格式项参数,如'Y-m-d'
        while (n < m_pattern.size())
        {
            // 如果不是字母且不是'{'或'}',并且不在解析参数状态,说明格式项标识符结束
            if (!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')
            {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if (fmt_status == 0)
            {
                // 遇到'{',表示开始解析参数
                if (m_pattern[n] == '{')
                {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // 状态切换为解析参数
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            }
            else if (fmt_status == 1)
            {
                // 遇到'}',表示参数解析结束
                if (m_pattern[n] == '}')
                {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 2; // 状态切换为参数解析完毕
                    ++n;
                    break;
                }
            }
            ++n;
            // 如果解析到字符串末尾,仍未结束,则将剩余部分作为标识符
            if (n == m_pattern.size())
            {
                if (str.empty())
                {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if (fmt_status == 0) // 普通格式项,无参数
        {
            vec.emplace_back(str, "", 1);
            i = n - 1;
        }
        else if (fmt_status == 1) // 参数解析未正常结束(如'{'未闭合)
        {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.emplace_back("<<pattern_error>>", "", 0);
        }
        else if (fmt_status == 2) // 带参数的格式项
        {
            vec.emplace_back(str, fmt, 1);
            i = n - 1;
        }
    }

    // 如果模板以普通字符串结尾,将其存入vec
    if (!nstr.empty())
    {
        vec.emplace_back(nstr, "", 0);
    }

    // 使用静态map和lambda表达式实现工厂模式,将字符串标识符映射到对应的FormatItem构造函数
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> s_format_items = {
// 宏定义简化了向map中添加条目的代码
#define XX(str, C) {str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }}
#define XX_SIMPLE(str, C) {str, [](const std::string &fmt) { return FormatItem::ptr(new C()); }}

        XX("d", DateTimeFormatItem),        // %d: 日期时间
        XX_SIMPLE("m", MessageFormatItem),  // %m: 日志消息
        XX_SIMPLE("p", LevelFormatItem),    // %p: 日志级别
        XX_SIMPLE("c", NameFormatItem),     // %c: 日志器名称
        XX_SIMPLE("t", ThreadIdFormatItem), // %t: 线程ID
        XX_SIMPLE("n", NewLineFormatItem),  // %n: 换行
        XX_SIMPLE("f", FilenameFormatItem), // %f: 文件名
        XX_SIMPLE("l", LineFormatItem),     // %l: 行号
        XX_SIMPLE("T", TabFormatItem)       // %T: Tab
#undef XX
#undef XX_SIMPLE
    };

    // 遍历解析结果vec,利用工厂s_format_items创建具体的FormatItem对象实例
    for (auto &i : vec)
    {
        if (std::get<2>(i) == 0) // 类型为0,是普通字符串
        {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        }
        else // 类型为1,是格式化项
        {
            auto it = s_format_items.find(std::get<0>(i));
            if (it == s_format_items.end()) // 未知的格式化项
            {
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            }
            else
            {
                // 调用工厂函数,创建对应的FormatItem实例
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }
    }
}