#include "LogManager.h"
#include "AsyncLogging.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib> // For system()

// 全局异步日志对象，仅在LogManager内部使用
static std::unique_ptr<AsyncLogging> g_asyncLog;

// 声明外部的函数指针变量
extern void (*g_asyncOutputFunc)(const char *msg, int len);

// 监控线程是否正在运行的标志
static std::atomic<bool> g_monitorRunning(false);
// 监控线程
static std::thread g_monitorThread;

/**
 * @brief 监控异步日志系统的健康状态
 * @param checkInterval 检查间隔(秒)
 *
 * 定期检查异步日志系统是否正常工作，如果发现问题，输出警告并尝试恢复
 */
static void monitorAsyncLogging(int checkInterval)
{
    // 连续失败计数
    int failureCount = 0;
    // 上次尝试恢复的时间
    auto lastRecoveryAttempt = std::chrono::steady_clock::now();

    while (g_monitorRunning)
    {
        bool hasError = false;

        // 检查异步日志对象是否存在
        if (!g_asyncLog)
        {
            std::cerr << "警告: 异步日志对象不存在" << std::endl;
            hasError = true;
        }

        // 检查异步输出函数是否设置
        if (!g_asyncOutputFunc)
        {
            std::cerr << "警告: 异步输出函数未设置，日志系统可能降级为同步模式" << std::endl;
            hasError = true;
        }

        if (hasError)
        {
            failureCount++;

            // 如果连续失败次数达到阈值，尝试恢复
            if (failureCount >= 3)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastRecoveryAttempt).count();

                // 至少间隔5分钟才尝试恢复，避免频繁重启
                if (elapsed >= 5)
                {
                    std::cerr << "尝试恢复异步日志系统..." << std::endl;

                    // 尝试重新初始化
                    if (LogManager::getInstance().reinitializeAsyncLogging())
                    {
                        std::cerr << "异步日志系统恢复成功" << std::endl;
                        failureCount = 0; // 重置失败计数
                    }
                    else
                    {
                        std::cerr << "异步日志系统恢复失败" << std::endl;
                    }

                    lastRecoveryAttempt = now;
                }
            }
        }
        else
        {
            // 系统正常，重置失败计数
            if (failureCount > 0)
            {
                std::cerr << "异步日志系统恢复正常" << std::endl;
                failureCount = 0;
            }
        }

        // 休眠指定时间
        std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
    }
}

/**
 * @brief 启动异步日志监控线程
 * @param checkInterval 检查间隔(秒)
 */
static void startMonitor(int checkInterval = 60)
{
    if (!g_monitorRunning)
    {
        g_monitorRunning = true;
        g_monitorThread = std::thread(monitorAsyncLogging, checkInterval);
        g_monitorThread.detach(); // 分离线程，让它在后台运行
    }
}

/**
 * @brief 停止异步日志监控线程
 */
static void stopMonitor()
{
    g_monitorRunning = false;
    // 线程已经detach，无需join
}

/**
 * @brief LogManager构造函数
 * 初始化根日志器(root)，为其配置一个默认的控制台输出Appender
 * 并默认启用异步日志
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

    // 默认启用异步日志
    init("./logs/app", 10 * 1024 * 1024, 1);

    // 启动监控线程，每60秒检查一次
    startMonitor(60);
}

/**
 * @brief 析构函数，确保资源正确释放
 */
LogManager::~LogManager()
{
    // 停止监控线程
    stopMonitor();

    // 停止异步日志
    if (g_asyncLog)
    {
        g_asyncLog->stop();
        g_asyncLog.reset();
    }
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
void LogManager::init(const std::string &asyncLogBasename,
                      off_t asyncLogRollSize,
                      int asyncLogFlushInterval)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 防止重复初始化
    if (m_initialized)
    {
        std::cerr << "日志系统已初始化，不能重复初始化" << std::endl;
        return;
    }

    // 增加缓冲区大小，从默认的4KB提高到8KB
    g_asyncLog = std::make_unique<AsyncLogging>(
        asyncLogBasename,
        asyncLogRollSize,
        asyncLogFlushInterval,
        8192); // 增加缓冲区参数

// 生产环境调整默认级别为INFO
#ifdef NDEBUG // 发布模式
    m_root->setLevel(Level::INFO);
#else // 调试模式
    m_root->setLevel(Level::DEBUG);
#endif

    // 创建日志目录
    if (!asyncLogBasename.empty())
    {
        size_t pos = asyncLogBasename.find_last_of('/');
        if (pos != std::string::npos)
        {
            std::string dir = asyncLogBasename.substr(0, pos);
            std::string cmd = "mkdir -p " + dir;
            system(cmd.c_str());
        }
    }

    // 若指定了异步日志文件名，则启用异步日志
    if (!asyncLogBasename.empty())
    {
        try
        {
            // 创建异步日志实例
            g_asyncLog = std::make_unique<AsyncLogging>(asyncLogBasename, asyncLogRollSize, asyncLogFlushInterval, 8192); // 增加缓冲区参数
            g_asyncLog->start();

            // 设置全局异步输出函数
            g_asyncOutputFunc = [](const char *msg, int len)
            {
                if (g_asyncLog)
                {
                    g_asyncLog->append(msg, len);
                }
                else
                {
                    // 异步日志对象不存在，回退到标准错误输出
                    std::cerr.write(msg, len);
                    std::cerr << "[日志系统错误: 异步日志对象不存在]" << std::endl;
                }
            };

            // 添加文件输出器到root日志器
            LogAppenderPtr fileAppender(new FileLogAppender(asyncLogBasename + ".log"));
            fileAppender->setFormatter(
                std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
            m_root->addAppender(fileAppender);

            // 记录启动信息
            auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 1, time(0), Level::INFO);
            event->getStringStream() << "异步日志系统已启动 - 文件: " << asyncLogBasename
                                     << ", 滚动大小: " << (asyncLogRollSize / 1024 / 1024) << "MB"
                                     << ", 刷新间隔: " << asyncLogFlushInterval << "秒";
            m_root->info(event);
        }
        catch (const std::exception &e)
        {
            std::cerr << "异步日志系统初始化失败: " << e.what() << std::endl;
            std::cerr << "回退到同步日志模式" << std::endl;

            // 添加文件输出器（同步模式）
            try
            {
                LogAppenderPtr fileAppender(new FileLogAppender(asyncLogBasename + ".log"));
                fileAppender->setFormatter(
                    std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
                m_root->addAppender(fileAppender);
            }
            catch (const std::exception &e)
            {
                std::cerr << "同步日志初始化也失败: " << e.what() << std::endl;
            }
        }
    }
    else
    {
        std::cerr << "警告: 未指定日志文件名，日志将只输出到控制台" << std::endl;
    }

    m_initialized = true;
}

/**
 * @brief 检查异步日志系统状态
 * @return true表示异步日志系统正常工作，false表示异常
 */
bool LogManager::checkAsyncLoggingStatus() const
{
    if (!g_asyncLog || !g_asyncOutputFunc)
    {
        return false;
    }
    return true;
}

/**
 * @brief 重新初始化异步日志系统
 * 在监控到异常状态时调用此方法尝试恢复
 * @return true表示重新初始化成功，false表示失败
 */
bool LogManager::reinitializeAsyncLogging()
{
    if (m_initialized)
    {
        // 停止现有的异步日志
        if (g_asyncLog)
        {
            g_asyncLog->stop();
            g_asyncLog.reset();
        }

        // 重置初始化标志
        m_initialized = false;

        // 重新初始化
        init("./logs/app", 10 * 1024 * 1024, 1);

        return m_initialized;
    }
    return false;
}