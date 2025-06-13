#include "LogAppender.h"
#include "Logger.h"
#include "AsyncLogging.h"
#include <iostream>
#include <functional>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <system_error>

/**
 * @brief 全局异步输出函数指针，初始为nullptr
 * 可以被外部代码设置，用于替换默认的异步输出行为
 */
void (*g_asyncOutputFunc)(const char *msg, int len) = nullptr;

/**
 * @brief 默认的异步输出处理函数
 * 当g_asyncOutputFunc为nullptr时使用此函数
 * @param msg 日志消息
 * @param len 消息长度
 */
void defaultAsyncOutput(const char *msg, int len)
{
    // 默认实现为空
    // 在生产环境中，这里可能会记录错误或执行备用操作
}

/**
 * @brief 异步输出函数的外部接口
 * 此函数是外部代码调用的接口点，内部会根据g_asyncOutputFunc是否设置决定实际调用哪个函数
 * @param msg 日志消息
 * @param len 消息长度
 */
void asyncOutput(const char *msg, int len)
{
    if (g_asyncOutputFunc)
    {
        g_asyncOutputFunc(msg, len); // 使用设置的函数处理
    }
    else
    {
        defaultAsyncOutput(msg, len); // 使用默认实现
    }
}

// --- LogAppender 基类实现 ---
void LogAppender::setFormatter(LogFormatter::ptr formatter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_formatter = formatter;
}

LogFormatter::ptr LogAppender::getFormatter() const
{
    return m_formatter;
}

void LogAppender::addFilter(LogFilter::ptr filter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filters.push_back(filter);
}

void LogAppender::clearFilters()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filters.clear();
}

bool LogAppender::shouldFilter(LogEvent::ptr event) const
{
    // 检查是否被过滤器过滤
    for (auto &filter : m_filters)
    {
        if (filter->filter(event))
        {
            return true; // 应该被过滤
        }
    }
    return false; // 不过滤
}

// --- StdoutLogAppender 实现 ---
StdoutLogAppender::StdoutLogAppender()
{
    // 设置一个默认的格式化器
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n"));
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogEvent::ptr event)
{
    // 首先检查级别过滤
    if (event->getLevel() >= m_level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 检查是否应该过滤
        if (shouldFilter(event))
        {
            return;
        }

        // 格式化并输出
        if (m_formatter)
        {
            std::cout << m_formatter->format(logger, event);
        }
    }
}

// --- FileLogAppender 实现 ---
FileLogAppender::FileLogAppender(const std::string &filename)
    : m_filename(filename)
{
    // 每个Appender都应该有一个默认的Formatter
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n"));

    // 确保目录存在
    size_t pos = filename.find_last_of('/');
    if (pos != std::string::npos)
    {
        std::string dir = filename.substr(0, pos);
        // 使用系统命令创建目录
        std::string cmd = "mkdir -p " + dir;
        system(cmd.c_str());
    }

    // 打开文件
    m_filestream.open(filename, std::ios::app);
    if (!m_filestream)
    {
        std::error_code ec(errno, std::system_category());
        std::cerr << "无法打开日志文件: " << filename
                  << ", 错误: " << ec.value()
                  << " (" << ec.message() << ")" << std::endl;

        // 尝试创建一个空文件
        std::string testCmd = "touch " + filename;
        system(testCmd.c_str());

        // 再次尝试打开
        m_filestream.clear();
        m_filestream.open(filename, std::ios::app);
        if (!m_filestream)
        {
            std::error_code ec2(errno, std::system_category());
            std::cerr << "重试打开文件仍然失败: " << filename
                      << ", 错误: " << ec2.value()
                      << " (" << ec2.message() << ")" << std::endl;
        }
    }
}

FileLogAppender::~FileLogAppender()
{
    if (m_filestream.is_open())
    {
        m_filestream.close();
    }
}

void FileLogAppender::log(Logger::ptr logger, LogEvent::ptr event)
{
    // 首先检查级别过滤
    if (event->getLevel() >= m_level)
    {
        // 线程安全加锁
        std::lock_guard<std::mutex> lock(m_mutex);

        // 检查是否应该过滤
        if (shouldFilter(event))
        {
            return;
        }

        // 格式化日志
        if (m_formatter)
        {
            std::string msg = m_formatter->format(logger, event);

            // 优先尝试通过异步方式写入
            if (g_asyncOutputFunc)
            {
                asyncOutput(msg.c_str(), msg.length());
            }
            // 如果未设置异步输出，则直接写入文件
            else if (m_filestream.is_open())
            {
                m_filestream << msg;
                m_filestream.flush(); // 立即刷新缓冲区，确保写入

                // 检查写入是否成功
                if (m_filestream.fail())
                {
                    std::error_code ec(errno, std::system_category());
                    std::cerr << "写入日志文件失败: " << m_filename
                              << ", 错误: " << ec.value()
                              << " (" << ec.message() << ")" << std::endl;
                }
            }
            else
            {
                // 如果文件未打开，尝试重新打开
                m_filestream.clear();
                m_filestream.open(m_filename, std::ios::app);
                if (m_filestream.is_open())
                {
                    m_filestream << msg;
                    m_filestream.flush();

                    if (m_filestream.fail())
                    {
                        std::error_code ec(errno, std::system_category());
                        std::cerr << "重新打开后写入失败: " << m_filename
                                  << ", 错误: " << ec.value()
                                  << " (" << ec.message() << ")" << std::endl;
                    }
                }
                else
                {
                    std::error_code ec(errno, std::system_category());
                    std::cerr << "重新打开文件失败: " << m_filename
                              << ", 错误: " << ec.value()
                              << " (" << ec.message() << ")" << std::endl;
                }
            }
        }
    }
}

bool FileLogAppender::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 关闭现有文件（如果已打开）
    if (m_filestream.is_open())
    {
        m_filestream.close();
    }

    // 尝试重新打开
    m_filestream.open(m_filename, std::ios::app);
    return m_filestream.is_open();
}
