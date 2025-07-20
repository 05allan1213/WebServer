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
 *
 * 这是一个函数指针变量，允许在运行时动态替换异步日志的输出行为。
 * 通常在初始化异步日志系统时，会将其设置为指向AsyncLogging::append方法。
 */
void (*g_asyncOutputFunc)(const char *msg, int len) = nullptr;

/**
 * @brief 默认的异步输出处理函数
 * 当g_asyncOutputFunc为nullptr时使用此函数
 * @param msg 日志消息内容
 * @param len 消息长度
 *
 * 这个函数作为异步日志输出的备用方案，在异步日志系统未正确配置时触发。
 * 它将日志输出到标准错误流，并在累计一定次数后输出警告信息。
 */
void defaultAsyncOutput(const char *msg, int len)
{
    // 记录失败次数，用于统计和警告
    // 使用静态变量在函数调用之间保持状态
    static int failureCount = 0;
    failureCount++;

    // 始终输出到标准错误流，确保日志不会丢失
    // 直接使用write方法写入，避免格式化开销
    std::cerr.write(msg, len);

    // 每达到一定次数（如1000次）输出一次警告信息
    // 避免每次都输出警告，减少标准错误流的负担
    if (failureCount % 1000 == 0)
    {
        std::string warning = "\n警告: 异步日志系统未配置，已回退到标准错误输出 " + std::to_string(failureCount) + " 次\n";
        std::cerr << warning;
    }

    // 备用输出方案: 写入备用日志文件
    // static std::ofstream fallbackLog("../logs/fallback.log", std::ios::app);
    // if (fallbackLog.is_open()) {
    //     fallbackLog.write(msg, len);
    //     fallbackLog.flush();
    // }
}

/**
 * @brief 异步输出函数的外部接口
 * 此函数是外部代码调用的接口点，内部会根据g_asyncOutputFunc是否设置决定实际调用哪个函数
 * @param msg 日志消息内容
 * @param len 消息长度
 *
 * 这是一个门面函数，根据g_asyncOutputFunc是否被设置，选择调用自定义的输出函数或默认函数。
 * 这种设计使得日志系统的输出行为可以在不修改代码的情况下动态切换。
 */
void asyncOutput(const char *msg, int len)
{
    if (g_asyncOutputFunc)
    {
        // 如果设置了自定义输出函数，则调用它
        g_asyncOutputFunc(msg, len);
    }
    else
    {
        // 否则使用默认实现
        defaultAsyncOutput(msg, len);
    }
}

// ---------------------- LogAppender 基类实现 ----------------------

/**
 * @brief 设置日志格式化器
 * @param formatter 格式化器指针
 *
 * 线程安全地设置Appender的格式化器。
 * 每个Appender可以有自己的格式化器，覆盖Logger的默认格式化器。
 */
void LogAppender::setFormatter(LogFormatter::ptr formatter)
{
    // 使用互斥锁保证线程安全
    std::lock_guard<std::mutex> lock(m_mutex);
    m_formatter = formatter;
}

/**
 * @brief 获取日志格式化器
 * @return 格式化器指针
 *
 * 返回当前Appender使用的格式化器。
 * @warning 此方法不加锁，在多线程环境下可能返回正在被修改的格式化器。
 */
LogFormatter::ptr LogAppender::getFormatter() const
{
    return m_formatter;
}

/**
 * @brief 添加日志过滤器
 * @param filter 过滤器指针
 *
 * 线程安全地向Appender添加一个过滤器。
 * 过滤器用于根据特定条件决定是否输出日志。
 */
void LogAppender::addFilter(LogFilter::ptr filter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filters.push_back(filter);
}

/**
 * @brief 清除所有过滤器
 *
 * 线程安全地移除Appender的所有过滤器。
 */
void LogAppender::clearFilters()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filters.clear();
}

/**
 * @brief 检查是否应该过滤本条日志
 * @param event 日志事件
 * @return true表示应该过滤掉(不输出)，false表示不过滤(正常输出)
 *
 * 遍历所有过滤器，如果任一过滤器返回true，则过滤该日志。
 * 这实现了过滤器的"或"逻辑：任一过滤器匹配则过滤。
 */
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

// ---------------------- StdoutLogAppender 实现 ----------------------

/**
 * @brief StdoutLogAppender构造函数
 *
 * 创建一个输出到标准输出流的日志输出器，并设置默认格式化器。
 */
StdoutLogAppender::StdoutLogAppender()
{
    // 设置一个默认的格式化器
    // 格式：时间 [级别] 日志器名称: 消息内容
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n"));
}

/**
 * @brief 输出日志到控制台
 * @param logger 产生日志的日志器
 * @param event 日志事件
 *
 * 将日志输出到标准输出流（控制台）。
 * 首先检查日志级别和过滤器，然后使用格式化器格式化日志并输出。
 */
void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogEvent::ptr event)
{
    // 首先检查级别过滤：只有当日志事件级别高于或等于设定级别时才输出
    if (event->getLevel() >= m_level)
    {
        // 加锁保证线程安全，避免多线程输出时混乱
        std::lock_guard<std::mutex> lock(m_mutex);

        // 检查是否应该被过滤器过滤
        if (shouldFilter(event))
        {
            return; // 如果应该被过滤，直接返回不输出
        }

        // 格式化并输出到标准输出流
        if (m_formatter)
        {
            std::cout << m_formatter->format(logger, event);
        }
    }
}

// ---------------------- FileLogAppender 实现 ----------------------

/**
 * @brief FileLogAppender构造函数
 * @param filename 日志文件名
 *
 * 创建一个输出到文件的日志输出器。
 * 构造时会尝试创建必要的目录，并打开指定的日志文件。
 */
FileLogAppender::FileLogAppender(const std::string &filename)
    : m_filename(filename)
{
    // 设置一个默认的格式化器
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n"));

    // 如果已经启用了全局异步输出函数，则只负责将日志转发给异步系统，
    // 不再打开本地文件，避免生成重复的同步日志文件。
    if (g_asyncOutputFunc)
    {
        return;
    }

    // 确保目录存在
    size_t pos = filename.find_last_of('/');
    if (pos != std::string::npos)
    {
        // 提取目录部分
        std::string dir = filename.substr(0, pos);
        // 使用系统命令创建目录（支持递归创建）
        std::string cmd = "mkdir -p " + dir;
        system(cmd.c_str());
    }

    // 以追加模式打开文件
    m_filestream.open(filename, std::ios::app);
    if (!m_filestream)
    {
        // 打开失败，记录错误信息
        std::error_code ec(errno, std::system_category());
        std::cerr << "无法打开日志文件: " << filename
                  << ", 错误: " << ec.value()
                  << " (" << ec.message() << ")" << std::endl;

        // 尝试创建一个空文件
        std::string testCmd = "touch " + filename;
        system(testCmd.c_str());

        // 再次尝试打开
        m_filestream.clear(); // 清除错误状态
        m_filestream.open(filename, std::ios::app);
        if (!m_filestream)
        {
            // 如果再次失败，记录更多错误信息
            std::error_code ec2(errno, std::system_category());
            std::cerr << "重试打开文件仍然失败: " << filename
                      << ", 错误: " << ec2.value()
                      << " (" << ec2.message() << ")" << std::endl;
        }
    }
}

/**
 * @brief FileLogAppender析构函数
 *
 * 关闭文件流，确保所有日志都被写入磁盘。
 */
FileLogAppender::~FileLogAppender()
{
    // 如果文件流已打开，则关闭它
    if (m_filestream.is_open())
    {
        m_filestream.close();
    }
}

/**
 * @brief 输出日志到文件
 * @param logger 产生日志的日志器
 * @param event 日志事件
 *
 * 将日志输出到文件。支持两种模式：
 * 1. 异步模式：通过asyncOutput函数输出
 * 2. 同步模式：直接写入文件流
 *
 * 函数包含完善的错误处理，在文件写入失败时会尝试重新打开文件。
 * 高级别日志（ERROR、FATAL）会立即刷新，确保重要信息不丢失。
 */
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
            return; // 如果应该被过滤，直接返回不输出
        }

        // 格式化日志
        if (m_formatter)
        {
            // 将日志事件格式化为字符串
            std::string msg = m_formatter->format(logger, event);

            // 优先尝试通过异步方式写入
            if (g_asyncOutputFunc)
            {
                asyncOutput(msg.c_str(), msg.length());

                // 对于ERROR和FATAL级别的日志，强制刷新
                if (event->getLevel() >= Level::ERROR)
                {
                    // 这里不能直接调用flush，因为异步写入是由另一个线程处理的
                    // 但可以在异步输出函数中处理高优先级日志的立即刷新
                    // 这里我们添加一个特殊标记，让异步线程知道这是高优先级日志
                    std::string flushMarker = "##FLUSH_NOW##\n";
                    asyncOutput(flushMarker.c_str(), flushMarker.length());
                }
            }
            // 如果未设置异步输出，则直接写入文件（同步模式）
            else if (m_filestream.is_open())
            {
                m_filestream << msg;

                // 对于ERROR和FATAL级别的日志，立即刷新
                if (event->getLevel() >= Level::ERROR)
                {
                    m_filestream.flush();
                }
                else
                {
                    // 对于其他级别，根据时间间隔刷新
                    static time_t lastFlushTime = 0;
                    time_t now = time(nullptr);

                    // WARN级别每2秒刷新一次，其他级别每5秒刷新一次
                    int flushInterval = (event->getLevel() == Level::WARN) ? 2 : 5;

                    if (now - lastFlushTime >= flushInterval)
                    {
                        m_filestream.flush();
                        lastFlushTime = now;
                    }
                }

                // 检查写入是否成功
                if (m_filestream.fail())
                {
                    // 写入失败，记录错误信息
                    std::error_code ec(errno, std::system_category());
                    std::cerr << "写入日志文件失败: " << m_filename
                              << ", 错误: " << ec.value()
                              << " (" << ec.message() << ")" << std::endl;
                }
            }
            else
            {
                // 如果文件未打开，尝试重新打开
                // 这处理了文件可能被外部关闭或移动的情况
                m_filestream.clear(); // 清除错误状态
                m_filestream.open(m_filename, std::ios::app);
                if (m_filestream.is_open())
                {
                    // 重新打开成功，写入日志
                    m_filestream << msg;

                    // 对于ERROR和FATAL级别的日志，立即刷新
                    if (event->getLevel() >= Level::ERROR)
                    {
                        m_filestream.flush();
                    }

                    // 再次检查写入是否成功
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
                    // 重新打开也失败，记录错误
                    std::error_code ec(errno, std::system_category());
                    std::cerr << "重新打开文件失败: " << m_filename
                              << ", 错误: " << ec.value()
                              << " (" << ec.message() << ")" << std::endl;
                }
            }
        }
    }
}

/**
 * @brief 重新打开日志文件
 * @return 是否成功打开
 *
 * 关闭并重新打开日志文件。
 * 这在日志文件被外部程序移动或删除后很有用（如日志轮转）。
 */
bool FileLogAppender::reopen()
{
    // 加锁保证线程安全
    std::lock_guard<std::mutex> lock(m_mutex);

    // 关闭现有文件（如果已打开）
    if (m_filestream.is_open())
    {
        m_filestream.close();
    }

    // 尝试重新打开文件
    m_filestream.open(m_filename, std::ios::app);
    // 返回是否成功打开
    return m_filestream.is_open();
}

/**
 * @brief 设置日志滚动模式
 * @param mode 滚动模式
 */
void FileLogAppender::setRollMode(LogFile::RollMode mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rollMode = mode;

    // 记录日志滚动模式变更
    std::string modeStr;
    switch (mode)
    {
    case LogFile::RollMode::SIZE:
        modeStr = "按大小滚动";
        break;
    case LogFile::RollMode::DAILY:
        modeStr = "每天滚动";
        break;
    case LogFile::RollMode::HOURLY:
        modeStr = "每小时滚动";
        break;
    case LogFile::RollMode::MINUTELY:
        modeStr = "每分钟滚动";
        break;
    case LogFile::RollMode::SIZE_DAILY:
        modeStr = "综合策略：按大小和每天滚动";
        break;
    case LogFile::RollMode::SIZE_HOURLY:
        modeStr = "综合策略：按大小和每小时滚动";
        break;
    case LogFile::RollMode::SIZE_MINUTELY:
        modeStr = "综合策略：按大小和每分钟滚动";
        break;
    default:
        modeStr = "未知模式";
        break;
    }

    // 这里我们只能在文件流中记录这个变更，因为我们没有直接访问LogFile的方法
    if (m_filestream.is_open())
    {
        m_filestream << "--- 日志滚动模式已更改为: " << modeStr << " ---" << std::endl;
    }
}

/**
 * @brief 获取当前日志滚动模式
 * @return 当前滚动模式
 */
LogFile::RollMode FileLogAppender::getRollMode() const
{
    return m_rollMode;
}
