#include "LogEventWrap.h"

/**
 * @brief LogEventWrap构造函数
 * @param logger 日志器指针
 * @param event 日志事件指针
 */
LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
    : m_logger(logger), m_event(event)
{
}

/**
 * @brief LogEventWrap析构函数
 * @details 在析构时自动调用logger的log方法，提交日志事件，实现RAII自动提交
 */
LogEventWrap::~LogEventWrap()
{
    // 在析构时,调用 logger 的 log 方法,把 event 交给 logger 处理
    m_logger->log(m_event->getLevel(), m_event);
}

/**
 * @brief 获取日志事件的字符串流
 * @return 字符串流引用，可用于流式写入日志内容
 */
std::stringstream &LogEventWrap::getStringStream()
{
    return m_event->getStringStream();
}