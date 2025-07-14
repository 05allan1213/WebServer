#include "LogEvent.h"

LogEvent::LogEvent(const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint64_t time, Level level, const std::string &loggerName)
    : m_file(file), m_line(line), m_elapse(elapse), m_threadId(threadId), m_time(time), m_level(level), m_loggerName(loggerName)
{
}

LogEvent::~LogEvent()
{
}
