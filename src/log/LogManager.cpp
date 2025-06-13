#include "LogManager.h"
#include "AsyncLogging.h"
#include <iostream>
#include <memory>

// 全局异步日志对象，仅在LogManager内部使用
static std::unique_ptr<AsyncLogging> g_asyncLog;

// 声明外部的函数指针变量
extern void (*g_asyncOutputFunc)(const char *msg, int len);

/**
 * @brief LogManager构造函数
 * 初始化根日志器(root)，为其配置一个默认的控制台输出Appender
 */
LogManager::LogManager()
{
    // 创建默认的root日志器
    m_root = std::make_shared<Logger>("root");

    // 默认情况下，root日志器使用控制台输出
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
    m_root->addAppender(consoleAppender);

    // 将root日志器加入管理表
    m_loggers["root"] = m_root;
}

/**
 * @brief 获取LogManager单例
 * 使用C++11的局部静态变量特性实现线程安全的单例模式
 * @return LogManager单例的引用
 */
LogManager &LogManager::getInstance()
{
    // 线程安全的单例实现
    static LogManager instance;
    return instance;
}

/**
 * @brief 获取或创建指定名称的日志器
 * 如果请求的日志器不存在，则创建一个新的日志器
 * 新创建的日志器会继承root日志器的配置
 * @param name 日志器名称
 * @return 日志器智能指针
 */
Logger::ptr LogManager::getLogger(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 查找是否已存在该名称的Logger
    auto it = m_loggers.find(name);
    if (it != m_loggers.end())
    {
        return it->second;
    }

    // 不存在则创建一个新的Logger
    Logger::ptr logger = std::make_shared<Logger>(name);

    // 新创建的logger继承root的appender
    logger->setLevel(m_root->getLevel());
    if (m_root->getFormatter())
    {
        logger->setFormatter(m_root->getFormatter());
    }

    // 继承root的所有Appender
    for (auto appender : m_root->getAppenders())
    {
        logger->addAppender(appender);
    }

    // 加入管理表
    m_loggers[name] = logger;
    return logger;
}

/**
 * @brief 初始化日志系统
 * 配置异步日志和全局日志级别
 * @param asyncLogBasename 异步日志文件基础名，为空则不启用异步日志
 * @param asyncLogRollSize 日志文件滚动大小(字节)
 * @param asyncLogFlushInterval 日志刷新间隔(秒)
 */
void LogManager::init(const std::string &asyncLogBasename, off_t asyncLogRollSize, int asyncLogFlushInterval)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 防止重复初始化
    if (m_initialized)
    {
        std::cerr << "日志系统已初始化，不能重复初始化" << std::endl;
        return;
    }

    // 设置默认日志级别
    m_root->setLevel(Level::INFO);

    // 若指定了异步日志文件名，则启用异步日志
    if (!asyncLogBasename.empty())
    {
        // 创建异步日志实例
        g_asyncLog = std::make_unique<AsyncLogging>(
            asyncLogBasename,
            asyncLogRollSize,
            asyncLogFlushInterval);
        g_asyncLog->start();

        // 设置全局异步输出函数
        g_asyncOutputFunc = [](const char *msg, int len)
        {
            if (g_asyncLog)
            {
                g_asyncLog->append(msg, len);
            }
        };

        // 添加文件输出器到root日志器
        LogAppenderPtr fileAppender(new FileLogAppender(asyncLogBasename + ".log"));
        fileAppender->setFormatter(
            std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
        m_root->addAppender(fileAppender);
    }

    m_initialized = true;
}