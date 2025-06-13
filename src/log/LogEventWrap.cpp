#include "LogEventWrap.h"

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
    : m_logger(logger), m_event(event)
{
}

LogEventWrap::~LogEventWrap()
{
    // 在析构时，调用 logger 的 log 方法，把 event 交给 logger 处理
    m_logger->log(m_event->getLevel(), m_event);
}

std::stringstream &LogEventWrap::getStringStream()
{
    return m_event->getStringStream();
}