#include "LogEvent.h"

/**
 * @brief LogEvent构造函数
 * @param file 源文件名
 * @param line 行号
 * @param elapse 程序启动到现在的毫秒数
 * @param threadId 线程ID
 * @param time 时间戳
 * @param level 日志级别
 * @param loggerName 日志器名称
 */
LogEvent::LogEvent(const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint64_t time, Level level, const std::string &loggerName)
    : m_file(file), m_line(line), m_elapse(elapse), m_threadId(threadId), m_time(time), m_level(level), m_loggerName(loggerName)
{
}

/**
 * @brief LogEvent析构函数
 */
LogEvent::~LogEvent()
{
}
