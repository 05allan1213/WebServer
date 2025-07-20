#pragma once

#include "LogEvent.h"
#include "Logger.h"

/**
 * @brief 日志事件包装器,利用RAII机制自动提交日志
 *
 * 该类实现了RAII(资源获取即初始化)设计模式：
 * 1. 构造时：捕获Logger和LogEvent
 * 2. 析构时：自动调用logger->log()提交日志
 *
 * 这使得用户可以使用流式语法记录日志,而不必手动调用log方法
 * 例如: LOG_INFO(logger) << "message" << 123;
 */
class LogEventWrap
{
public:
    /**
     * @brief 构造函数,捕获Logger和LogEvent
     * @param logger 日志器指针
     * @param event 日志事件指针
     */
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

    /**
     * @brief 析构函数,负责真正的日志提交
     *
     * 当临时的LogEventWrap对象生命周期结束时,
     * 析构函数会自动调用logger->log()方法提交日志
     */
    ~LogEventWrap();

    /**
     * @brief 获取日志事件的字符串流,用于流式写入
     * @return 字符串流的引用
     */
    std::stringstream &getStringStream();

private:
    Logger::ptr m_logger;  // 日志器
    LogEvent::ptr m_event; // 日志事件
};