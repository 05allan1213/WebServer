#pragma once

#include "LogEvent.h"
#include "base/noncopyable.h"
#include <string>
#include <vector>
#include <memory>

class Logger;

/**
 * @brief 日志格式化项基类,定义了格式化接口
 *
 * FormatItem是一个抽象基类,实现了策略模式。
 * 不同的子类(如DateTimeFormatItem, LevelFormatItem等)
 * 分别实现了不同的格式化策略,处理不同的格式占位符(%d, %p等)
 */
class FormatItem : private noncopyable
{
public:
    using ptr = std::shared_ptr<FormatItem>;

    virtual ~FormatItem() {}

    /**
     * @brief 格式化接口,将日志事件按照特定格式输出到流中
     * @param os 输出流
     * @param logger 日志器
     * @param event 日志事件
     */
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogEvent::ptr event) = 0;
};

/**
 * @brief 日志格式化器,负责解析格式模板并实现格式化
 *
 * LogFormatter解析像"%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n"这样的格式字符串,
 * 并将每个格式项(如%d, %p等)解析为对应的FormatItem对象。
 * 在格式化时,依次调用各FormatItem的format方法,完成完整日志的格式化。
 */
class LogFormatter : private noncopyable
{
public:
    // 使用智能指针管理LogFormatter对象
    using ptr = std::shared_ptr<LogFormatter>;

public:
    /**
     * @brief 构造函数,指定格式化模板
     * @param pattern 格式化模板字符串,如"%d{%Y-%m-%d} [%p] %m%n"
     */
    LogFormatter(const std::string &pattern);

    /**
     * @brief 格式化日志事件
     * @param logger 日志器
     * @param event 日志事件
     * @return 格式化后的日志字符串
     */
    std::string format(std::shared_ptr<Logger> logger, LogEvent::ptr event);

private:
    /**
     * @brief 解析格式化模板,初始化FormatItem列表
     */
    void init();

private:
    std::string m_pattern;                // 格式化模板字符串
    std::vector<FormatItem::ptr> m_items; // 解析后得到的FormatItem集合
    bool m_error = false;                 // 模板是否存在错误
};