#pragma once

#include "LogLevel.h"
#include <string>
#include <cstdint>
#include <memory>
#include <sstream>

/**
 * @brief 日志事件类，封装单条日志的所有相关信息
 *
 * LogEvent是日志系统的核心数据类，包含了一条日志记录的所有元数据：
 * 文件名、行号、时间戳、线程ID、日志级别以及日志内容等。
 * 它是数据容器，不负责格式化或输出。
 */
class LogEvent
{
public:
    // 使用智能指针管理LogEvent对象
    using ptr = std::shared_ptr<LogEvent>;

public:
    /**
     * @brief 构造一个日志事件
     * @param file 源文件名
     * @param line 行号
     * @param elapse 程序启动到现在的毫秒数
     * @param threadId 线程ID
     * @param time 时间戳
     * @param level 日志级别
     */
    LogEvent(const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint64_t time, Level level, const std::string &loggerName);
    ~LogEvent();

    // 获取日志信息的各种访问器
    const char *getFile() const { return m_file; }
    int32_t getLine() const { return m_line; }
    uint32_t getElapse() const { return m_elapse; }
    uint32_t getThreadId() const { return m_threadId; }
    uint64_t getTime() const { return m_time; }
    Level getLevel() const { return m_level; }
    const std::string &getLoggerName() const { return m_loggerName; }

    /**
     * @brief 获取日志内容字符串流
     * @return 字符串流的引用，用于流式写入日志内容
     */
    std::stringstream &getStringStream() { return m_ss; }

private:
    const char *m_file = nullptr;  // 文件名
    int32_t m_line = 0;            // 行号
    uint32_t m_elapse = 0;         // 程序启动到现在的毫秒数
    uint32_t m_threadId = 0;       // 线程id
    uint64_t m_time = 0;           // 时间戳
    Level m_level = Level::UNKNOW; // 日志级别
    std::string m_loggerName;      // 日志器名称
    std::stringstream m_ss;        // 日志内容
};
