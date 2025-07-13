#include "LogFilter.h"

/**
 * @brief LevelFilter构造函数
 * @param level 过滤的最低日志级别
 *
 * 初始化级别过滤器，设置允许通过的最低日志级别
 */
LevelFilter::LevelFilter(Level level) : m_level(level) {}

/**
 * @brief 判断日志事件是否应该被过滤
 * @param event 待判断的日志事件
 * @return true表示应该过滤掉，false表示应该保留
 *
 * 实现原理：比较日志事件级别与设定的最低级别
 * 如果日志级别低于设定的最低级别，则返回true（过滤掉）
 */
bool LevelFilter::filter(LogEvent::ptr event)
{
    return event->getLevel() < m_level;
}

/**
 * @brief 获取过滤器名称
 * @return 过滤器名称字符串
 */
std::string LevelFilter::getName() const
{
    return "LevelFilter";
}

/**
 * @brief RegexFilter构造函数
 * @param pattern 正则表达式模式字符串
 * @param exclude 匹配行为标志，true表示匹配则过滤，false表示匹配则保留
 *
 * 初始化正则表达式过滤器，编译正则表达式模式并设置匹配行为
 */
RegexFilter::RegexFilter(const std::string &pattern, bool exclude)
    : m_regex(pattern), m_exclude(exclude) {}

/**
 * @brief 基于正则表达式判断日志是否应该被过滤
 * @param event 待判断的日志事件
 * @return true表示应该过滤掉，false表示应该保留
 *
 * 实现原理：
 * 1. 获取日志事件的完整内容字符串
 * 2. 使用正则表达式进行匹配
 * 3. 根据m_exclude标志决定匹配结果的含义：
 *    - 如果m_exclude为true，匹配成功则过滤（返回true）
 *    - 如果m_exclude为false，匹配成功则保留（返回false）
 */
bool RegexFilter::filter(LogEvent::ptr event)
{
    std::string content = event->getStringStream().str();
    bool match = std::regex_search(content, m_regex);
    // 如果m_exclude为true，匹配则过滤；否则匹配则保留
    return m_exclude ? match : !match;
}

/**
 * @brief 获取过滤器名称
 * @return 过滤器名称字符串
 */
std::string RegexFilter::getName() const
{
    return "RegexFilter";
}

/**
 * @brief FileFilter构造函数
 * @param filename 要匹配的文件名（部分匹配）
 * @param exclude 匹配行为标志，true表示匹配则过滤，false表示匹配则保留
 *
 * 初始化文件名过滤器，设置要匹配的文件名和匹配行为
 */
FileFilter::FileFilter(const std::string &filename, bool exclude)
    : m_filename(filename), m_exclude(exclude) {}

/**
 * @brief 基于文件名判断日志是否应该被过滤
 * @param event 待判断的日志事件
 * @return true表示应该过滤掉，false表示应该保留
 *
 * 实现原理：
 * 1. 获取日志事件的源文件名
 * 2. 检查是否包含指定的文件名子串
 * 3. 根据m_exclude标志决定匹配结果的含义：
 *    - 如果m_exclude为true，匹配成功则过滤
 *    - 如果m_exclude为false，匹配成功则保留
 */
bool FileFilter::filter(LogEvent::ptr event)
{
    std::string filename = event->getFile();
    bool match = filename.find(m_filename) != std::string::npos;
    return match == m_exclude;
}

/**
 * @brief 获取过滤器名称
 * @return 过滤器名称字符串
 */
std::string FileFilter::getName() const
{
    return "FileFilter";
}

/**
 * @brief CompositeFilter构造函数
 * @param all 组合逻辑标志，true表示AND逻辑，false表示OR逻辑
 *
 * 初始化复合过滤器，设置子过滤器的组合逻辑
 */
CompositeFilter::CompositeFilter(bool all) : m_all(all) {}

/**
 * @brief 添加子过滤器
 * @param filter 要添加的过滤器指针
 *
 * 将一个过滤器添加到复合过滤器的子过滤器列表中
 */
void CompositeFilter::addFilter(LogFilter::ptr filter)
{
    m_filters.push_back(filter);
}

/**
 * @brief 清空所有子过滤器
 *
 * 移除复合过滤器中的所有子过滤器
 */
void CompositeFilter::clearFilters()
{
    m_filters.clear();
}

/**
 * @brief 基于多个子过滤器判断日志是否应该被过滤
 * @param event 待判断的日志事件
 * @return true表示应该过滤掉，false表示应该保留
 *
 * 实现原理：
 * 1. 如果没有子过滤器，默认不过滤（返回false）
 * 2. 根据m_all标志使用不同的组合逻辑：
 *    - m_all为true时使用AND逻辑：所有子过滤器都不过滤才不过滤
 *    - m_all为false时使用OR逻辑：任一子过滤器不过滤则不过滤
 * 3. 短路逻辑：
 *    - AND逻辑下，任一子过滤器返回true（过滤），则整体返回true（过滤）
 *    - OR逻辑下，任一子过滤器返回false（不过滤），则整体返回false（不过滤）
 */
bool CompositeFilter::filter(LogEvent::ptr event)
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

/**
 * @brief 获取过滤器名称
 * @return 过滤器名称字符串
 */
std::string CompositeFilter::getName() const
{
    return "CompositeFilter";
}

/**
 * @brief FunctionFilter构造函数
 * @param func 过滤函数对象
 *
 * 初始化函数过滤器，设置用于过滤判断的函数对象
 * 函数对象应接受LogEvent::ptr参数并返回bool值
 */
FunctionFilter::FunctionFilter(FunctionFilter::FilterFunction func) : m_func(func) {}

/**
 * @brief 基于函数对象判断日志是否应该被过滤
 * @param event 待判断的日志事件
 * @return true表示应该过滤掉，false表示应该保留
 *
 * 实现原理：直接调用构造时提供的函数对象，将日志事件作为参数传入
 * 函数对象的返回值决定是否过滤该日志事件
 */
bool FunctionFilter::filter(LogEvent::ptr event)
{
    return m_func(event);
}

/**
 * @brief 获取过滤器名称
 * @return 过滤器名称字符串
 */
std::string FunctionFilter::getName() const
{
    return "FunctionFilter";
}