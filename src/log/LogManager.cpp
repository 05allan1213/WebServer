#include "LogManager.h"
#include "AsyncLogging.h"
#include "LogEvent.h"
#include "LogEventWrap.h"
#include "log/LogConfig.h"
#include "base/ConfigManager.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>

// 全局异步日志对象,仅在LogManager内部使用
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
 * 定期检查异步日志系统是否正常工作,如果发现问题,输出警告并尝试恢复
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

        // 获取LogManager实例,检查是否已初始化
        auto logManager = LogManager::getInstance();
        bool isInitialized = logManager->isInitialized();

        // 如果日志系统尚未初始化,则跳过检查
        if (!isInitialized)
        {
            // 日志系统未初始化,不需要报警
            std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
            continue;
        }

        // 检查异步日志对象是否存在
        if (!g_asyncLog)
        {
            std::cerr << "警告: 异步日志对象不存在" << std::endl;
            hasError = true;
        }

        // 检查异步输出函数是否设置
        if (!g_asyncOutputFunc)
        {
            std::cerr << "警告: 异步输出函数未设置,日志系统可能降级为同步模式" << std::endl;
            hasError = true;
        }

        if (hasError)
        {
            failureCount++;

            // 如果连续失败次数达到阈值,尝试恢复
            if (failureCount >= 3)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastRecoveryAttempt).count();

                // 至少间隔5分钟才尝试恢复,避免频繁重启
                if (elapsed >= 5)
                {
                    std::cerr << "尝试恢复异步日志系统..." << std::endl;

                    // 尝试重新初始化
                    if (LogManager::getInstance()->reinitializeAsyncLogging())
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
            // 系统正常,重置失败计数
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
        g_monitorThread.detach(); // 分离线程,让它在后台运行
    }
}

/**
 * @brief 停止异步日志监控线程
 */
static void stopMonitor()
{
    g_monitorRunning = false;
    // 线程已经detach,无需join
}

/**
 * @brief LogManager构造函数
 * 初始化根日志器(root)
 */
LogManager::LogManager()
{
    // 创建默认的root日志器,但不配置任何Appender
    m_root = std::make_shared<Logger>("root");
    m_loggers["root"] = m_root;
}

/**
 * @brief 析构函数,确保资源正确释放
 */
LogManager::~LogManager()
{
    // 停止监控线程
    stopMonitor();

    // 注销回调
    ConfigManager::getInstance().unregisterUpdateCallback("LogManager");

    // 停止异步日志
    if (g_asyncLog)
    {
        g_asyncLog->stop();
        g_asyncLog.reset();
    }
}

std::shared_ptr<LogManager> LogManager::s_instance = nullptr;

std::shared_ptr<LogManager> LogManager::getInstance()
{
    if (!s_instance)
    {
        s_instance = std::shared_ptr<LogManager>(new LogManager());
    }
    return s_instance;
}

/**
 * @brief 获取或创建指定名称的日志器
 * 如果请求的日志器不存在,则创建一个新的日志器
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

    // 建立父子关系
    // 如果日志器名称为 "a.b.c",则其父日志器为 "a.b"
    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos)
    {
        // 如果名称中没有'.',说明是顶级日志器,其父是root
        // (root本身在构造时已创建,所以这里的name不会是"root")
        logger->setParent(m_root);
    }
    else
    {
        // 名称中有'.', 则递归获取父日志器
        std::string parent_name = name.substr(0, pos);
        logger->setParent(getLogger(parent_name));
    }

    // 新创建的logger默认继承父logger的级别
    // Appender和Formatter不再需要复制,依赖事件向上传递
    logger->setLevel(logger->getParent()->getLevel());

    // 加入管理表
    m_loggers[name] = logger;
    return logger;
}

/**
 * @brief 初始化或重新配置日志系统
 *
 * 这是配置日志系统的唯一入口。它会清空所有现有配置,
 * 然后根据提供的参数重新设置。
 *
 * @param asyncLogBasename 异步日志文件基础名。如果为空,则只使用控制台输出。
 * @param asyncLogRollSize 日志文件滚动大小(字节)。
 * @param asyncLogFlushInterval 日志刷新间隔(秒)。
 * @param rollMode 日志滚动模式
 */
void LogManager::init(const std::string &asyncLogBasename,
                      off_t asyncLogRollSize,
                      int asyncLogFlushInterval,
                      LogFile::RollMode rollMode)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // --- 0. 保存配置 ---
    m_logBasename = asyncLogBasename;
    m_rollSize = asyncLogRollSize;
    m_flushInterval = asyncLogFlushInterval;
    m_rollMode = rollMode;

    // --- 1. 停止并清理旧资源 ---
    if (g_asyncLog)
    {
        g_asyncLog->stop();
        g_asyncLog.reset();
    }
    if (g_monitorRunning)
    {
        stopMonitor();
    }

    // --- 2. 清空所有日志器的Appender ---
    for (auto &pair : m_loggers)
    {
        pair.second->clearAppenders();
    }

    // 从ConfigManager获取最新的LogConfig
    auto logConfig = ConfigManager::getInstance().getLogConfig();
    if (!logConfig)
    {
        LOG_ERROR(m_root) << "[LogManager] init 失败: 无法获取 LogConfig";
        // 至少保证有控制台输出
        LogAppenderPtr consoleAppender(new StdoutLogAppender());
        consoleAppender->setFormatter(std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
        m_root->addAppender(consoleAppender);
        m_root->setLevel(Level::DEBUG);
        m_initialized = true;
        return;
    }

    bool enableFile = logConfig->getEnableFile();
    bool enableAsync = logConfig->getEnableAsync();
    std::string fileLevelStr = logConfig->getFileLevel();
    std::string consoleLevelStr = logConfig->getConsoleLevel();

    // 字符串转Level
    auto parseLevel = [](const std::string &str)
    {
        if (str == "DEBUG")
            return Level::DEBUG;
        if (str == "INFO")
            return Level::INFO;
        if (str == "WARN")
            return Level::WARN;
        if (str == "ERROR")
            return Level::ERROR;
        if (str == "FATAL")
            return Level::FATAL;
        return Level::DEBUG;
    };

    // --- 3. 设置默认控制台输出 ---
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(
        std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
    consoleAppender->setLevel(parseLevel(consoleLevelStr));
    m_root->addAppender(consoleAppender);

    // --- 4. 配置日志级别 ---
#ifdef NDEBUG
    m_root->setLevel(Level::INFO);
#else
    m_root->setLevel(Level::DEBUG);
#endif

    // --- 5. 根据配置决定文件输出方式 ---
    if (enableFile && !asyncLogBasename.empty())
    {
        // 创建日志目录
        size_t pos = asyncLogBasename.find_last_of('/');
        if (pos != std::string::npos)
        {
            std::string dir = asyncLogBasename.substr(0, pos);
            system(("mkdir -p " + dir).c_str());
        }

        if (enableAsync)
        {
            // -------- 仅异步输出 --------
            try
            {
                g_asyncLog = std::make_unique<AsyncLogging>(
                    asyncLogBasename, asyncLogRollSize, asyncLogFlushInterval, 8192);
                g_asyncLog->start();

                // 设置全局异步输出函数指针
                g_asyncOutputFunc = [](const char *msg, int len)
                {
                    if (g_asyncLog)
                        g_asyncLog->append(msg, len);
                };

                // 创建FileLogAppender,但由于g_asyncOutputFunc已设置,它不会打开本地文件,只转发到异步日志
                auto fileAppender = std::make_shared<FileLogAppender>(asyncLogBasename + ".log");
                fileAppender->setFormatter(
                    std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
                fileAppender->setRollMode(rollMode);
                fileAppender->setLevel(parseLevel(fileLevelStr));
                m_root->addAppender(fileAppender);

                LOG_INFO(m_root) << "异步日志系统已启动 - 文件: " << asyncLogBasename
                                 << ", 滚动大小: " << (asyncLogRollSize / 1024 / 1024) << "MB"
                                 << ", 刷新间隔: " << asyncLogFlushInterval << "秒";
            }
            catch (const std::exception &e)
            {
                g_asyncOutputFunc = nullptr;
                std::cerr << "异步日志系统初始化失败: " << e.what() << std::endl;
            }
        }
        else
        {
            // -------- 仅同步文件输出 --------
            auto fileAppender = std::make_shared<FileLogAppender>(asyncLogBasename + ".log");
            fileAppender->setFormatter(
                std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
            fileAppender->setRollMode(rollMode);
            fileAppender->setLevel(parseLevel(fileLevelStr));
            m_root->addAppender(fileAppender);

            LOG_INFO(m_root) << "同步日志系统已启动 - 文件: " << asyncLogBasename;
        }
    }

    // --- 6. 标记为已初始化并启动监控 ---
    m_initialized = true;
    startMonitor(60);

    // --- 7. 注册配置更新回调 ---
    ConfigManager::getInstance().registerUpdateCallback("LogManager", [this]()
                                                        { this->onConfigUpdate(); });
}

/**
 * @brief 检查异步日志系统状态
 * @return true表示异步日志系统正常工作,false表示异常
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
 * @return true表示重新初始化成功,false表示失败
 */
bool LogManager::reinitializeAsyncLogging()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 停止并清理旧资源
    if (g_asyncLog)
    {
        g_asyncLog->stop();
        g_asyncLog.reset();
        g_asyncOutputFunc = nullptr;
    }

    // 清空root日志器的Appender,准备重新添加
    m_root->clearAppenders();

    // 重新添加控制台Appender
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(
        std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n"));
    m_root->addAppender(consoleAppender);

    // 从ConfigManager获取最新的LogConfig
    auto logConfig = ConfigManager::getInstance().getLogConfig();
    if (!logConfig)
    {
        LOG_ERROR(m_root) << "[LogManager] reinitialize 失败: 无法获取 LogConfig";
        return false;
    }

    bool enableFile = logConfig->getEnableFile();
    bool enableAsync = logConfig->getEnableAsync();

    if (!enableFile)
    {
        return true; // 只保留控制台输出
    }

    if (!m_logBasename.empty())
    {
        if (enableAsync)
        {
            // -------- 重新创建异步日志 --------
            try
            {
                g_asyncLog = std::make_unique<AsyncLogging>(m_logBasename, m_rollSize, m_flushInterval, 8192);
                g_asyncLog->start();

                g_asyncOutputFunc = [](const char *msg, int len)
                {
                    if (g_asyncLog)
                        g_asyncLog->append(msg, len);
                };

                // 创建FileLogAppender,但由于g_asyncOutputFunc已设置,它不会打开本地文件,只转发到异步日志
                auto fileAppender = std::make_shared<FileLogAppender>(m_logBasename + ".log");
                fileAppender->setFormatter(
                    std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
                fileAppender->setRollMode(m_rollMode);
                m_root->addAppender(fileAppender);

                LOG_INFO(m_root) << "异步日志系统已根据新配置重新初始化";
            }
            catch (const std::exception &e)
            {
                g_asyncOutputFunc = nullptr;
                std::cerr << "异步日志系统重新初始化失败: " << e.what() << std::endl;
                return false;
            }
        }
        else
        {
            // -------- 重新创建同步文件输出 --------
            auto fileAppender = std::make_shared<FileLogAppender>(m_logBasename + ".log");
            fileAppender->setFormatter(
                std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n"));
            fileAppender->setRollMode(m_rollMode);
            m_root->addAppender(fileAppender);

            LOG_INFO(m_root) << "同步日志系统已根据新配置重新初始化";
        }
    }
    return true;
}

/**
 * @brief 设置日志滚动模式
 * @param mode 滚动模式
 */
void LogManager::setRollMode(LogFile::RollMode mode)
{
    // 先存储新的滚动模式
    m_rollMode = mode;

    // 如果日志系统已初始化,则触发重新初始化流程以应用新模式
    // 注意：这里不能在锁内调用reinitializeAsyncLogging,因为它内部也会获取锁,可能导致死锁
    if (m_initialized)
    {
        // 先记录日志,表明滚动模式已更改
        auto rootLogger = getRoot();
        if (rootLogger)
        {
            LOG_INFO(rootLogger) << "日志滚动模式已请求更改为: " << static_cast<int>(mode) << ",将重新初始化文件日志...";
        }

        // 在单独的线程中执行重新初始化,避免死锁
        std::thread([this]()
                    {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 短暂延迟,确保日志已写入
            this->reinitializeAsyncLogging(); })
            .detach();
    }
}

void LogManager::onConfigUpdate()
{
    LOG_INFO(m_root) << "[LogManager] 检测到配置更新，准备重新应用日志配置...";

    // 从ConfigManager获取最新的LogConfig
    auto logConfig = ConfigManager::getInstance().getLogConfig();
    if (!logConfig)
    {
        LOG_ERROR(m_root) << "[LogManager] 获取最新日志配置失败，取消更新";
        return;
    }

    // 解析新的日志级别
    auto parseLevel = [](const std::string &str)
    {
        if (str == "DEBUG")
            return Level::DEBUG;
        if (str == "INFO")
            return Level::INFO;
        if (str == "WARN")
            return Level::WARN;
        if (str == "ERROR")
            return Level::ERROR;
        if (str == "FATAL")
            return Level::FATAL;
        return Level::DEBUG;
    };

    Level newFileLevel = parseLevel(logConfig->getFileLevel());
    Level newConsoleLevel = parseLevel(logConfig->getConsoleLevel());

    LOG_INFO(m_root) << "[LogManager] 新的文件日志级别: " << logConfig->getFileLevel();
    LOG_INFO(m_root) << "[LogManager] 新的控制台日志级别: " << logConfig->getConsoleLevel();

    std::lock_guard<std::mutex> lock(m_mutex);

    // 更新所有Appender的级别
    if (m_root)
    {
        for (const auto &appender : m_root->getAppenders())
        {
            // 假设 FileLogAppender 和 StdoutLogAppender 可以通过某种方式识别
            // 这里简单处理：假定文件appender的文件名不为空
            auto fileAppender = std::dynamic_pointer_cast<FileLogAppender>(appender);
            if (fileAppender)
            {
                fileAppender->setLevel(newFileLevel);
            }
            else
            {
                appender->setLevel(newConsoleLevel);
            }
        }
    }
    LOG_INFO(m_root) << "[LogManager] 日志级别配置热重载完成";
}