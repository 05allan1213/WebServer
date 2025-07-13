#include "LogManager.h"
#include "LogEventWrap.h"
#include "LogAppender.h"
#include "AsyncLogging.h"
#include "LogFilter.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <iomanip>
#include <functional>

// 声明外部函数
extern void (*g_asyncOutputFunc)(const char *msg, int len);

/**
 * @brief 输出标题分隔符，美化测试输出
 * @param title 标题内容
 */
void printTitle(const std::string &title)
{
    std::cout << "\n======== " << title << " ========" << std::endl;
}

/**
 * @brief 测试基本日志功能
 * 演示如何获取日志器、设置级别和记录不同级别的日志
 */
void testBasicLogging()
{
    printTitle("基本日志功能测试");

    // 获取默认root日志器
    auto rootLogger = getLogger();
    LOG_INFO(rootLogger) << "这是来自root日志器的消息";

    // 创建模块专用日志器
    auto moduleLogger = getLogger("MyModule");
    LOG_INFO(moduleLogger) << "这是来自MyModule日志器的消息";

    // 设置日志级别并测试级别过滤
    std::cout << "\n设置日志级别为WARN，只有WARN及以上级别的日志会输出：" << std::endl;
    moduleLogger->setLevel(Level::WARN);

    LOG_DEBUG(moduleLogger) << "DEBUG级别日志 - 不会输出";
    LOG_INFO(moduleLogger) << "INFO级别日志 - 不会输出";
    LOG_WARN(moduleLogger) << "WARN级别日志 - 会输出";
    LOG_ERROR(moduleLogger) << "ERROR级别日志 - 会输出";
    LOG_FATAL(moduleLogger) << "FATAL级别日志 - 会输出";

    // 恢复日志级别
    moduleLogger->setLevel(Level::DEBUG);
}

/**
 * @brief 测试日志格式化功能
 * 演示如何自定义日志格式
 */
void testFormatting()
{
    printTitle("日志格式化功能测试");

    auto logger = getLogger("FormatTest");

    // 测试不同的日志格式

    // 1. 默认格式
    std::cout << "默认格式：" << std::endl;
    LOG_INFO(logger) << "这是默认格式的日志";

    // 2. 简洁格式
    std::cout << "\n简洁格式：" << std::endl;
    logger->setFormatter("%d{%H:%M:%S} [%-5p] %m%n");
    LOG_INFO(logger) << "这是简洁格式的日志";

    // 3. 详细格式（包含文件名和行号）
    std::cout << "\n详细格式：" << std::endl;
    logger->setFormatter("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [%t] %c %f:%l - %m%n");
    LOG_INFO(logger) << "这是详细格式的日志";

    // 恢复默认格式
    logger->setFormatter("%d{%Y-%m-%d %H:%M:%S} [%p] %c - %m%n");
}

/**
 * @brief 测试多输出目标功能
 * 演示如何将日志同时输出到控制台和文件
 */
void testMultipleAppenders()
{
    printTitle("多输出目标功能测试");

    auto logger = getLogger("MultiAppenderTest");
    logger->clearAppenders();

    // 添加控制台输出
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [控制台] [%p] %m%n"));
    logger->addAppender(consoleAppender);

    // 添加文件输出
    LogAppenderPtr fileAppender(new FileLogAppender("../logs/multi_appender_test.log"));
    fileAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [文件] [%p] %m%n"));
    logger->addAppender(fileAppender);

    // 输出日志，将同时写入控制台和文件
    LOG_INFO(logger) << "这条日志会同时输出到控制台和文件";
    LOG_ERROR(logger) << "错误日志同样会输出到两个目标";

    std::cout << "* 日志已同时输出到控制台和logs/multi_appender_test.log文件" << std::endl;
}

/**
 * @brief 测试日志过滤器功能
 * 演示如何使用不同类型的过滤器过滤日志
 */
void testFilters()
{
    printTitle("日志过滤器功能测试");

    auto logger = getLogger("FilterTest");
    logger->clearAppenders();

    // 添加控制台输出
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [%p] %m%n"));
    logger->addAppender(consoleAppender);

    // 1. 级别过滤器
    std::cout << "级别过滤器测试（过滤DEBUG级别）：" << std::endl;
    LogFilter::ptr levelFilter = std::make_shared<LevelFilter>(Level::INFO);
    logger->addFilter(levelFilter);

    LOG_DEBUG(logger) << "DEBUG日志 - 应被过滤";
    LOG_INFO(logger) << "INFO日志 - 应正常输出";
    logger->clearFilters();

    // 2. 正则表达式过滤器
    std::cout << "\n正则过滤器测试（过滤包含'password'的日志）：" << std::endl;
    LogFilter::ptr regexFilter = std::make_shared<RegexFilter>("password", true);
    logger->addFilter(regexFilter);

    LOG_INFO(logger) << "普通日志 - 应正常输出";
    LOG_INFO(logger) << "含密码(password)的日志 - 应被过滤";
    logger->clearFilters();

    // 3. 自定义函数过滤器
    std::cout << "\n函数过滤器测试（过滤长度超过20的日志）：" << std::endl;
    LogFilter::ptr funcFilter = std::make_shared<FunctionFilter>(
        [](LogEvent::ptr event)
        {
            return event->getStringStream().str().length() > 20;
        });
    logger->addFilter(funcFilter);

    LOG_INFO(logger) << "短日志";
    LOG_INFO(logger) << "这是一条非常长的日志内容，应该被过滤掉";
    logger->clearFilters();
}

/**
 * @brief 测试异步日志功能
 * 演示异步日志的性能优势
 */
void testAsyncLogging()
{
    printTitle("异步日志功能测试");

    // 确保日志目录存在
    system("mkdir -p ../logs");

    // 初始化异步日志系统
    LogManager::getInstance().init("../logs/async_test", 1024 * 1024, 1);

    auto logger = getLogger("AsyncTest");

    // 测试异步日志性能
    const int LOG_COUNT = 10000;
    std::cout << "开始异步写入" << LOG_COUNT << "条日志..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // 快速写入大量日志
    for (int i = 0; i < LOG_COUNT; ++i)
    {
        LOG_INFO(logger) << "这是异步写入的第 " << i << " 条日志";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "写入" << LOG_COUNT << "条日志完成，耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "平均每条日志写入时间: " << std::fixed << std::setprecision(3)
              << (duration.count() / static_cast<double>(LOG_COUNT)) << "ms" << std::endl;
    std::cout << "日志已异步写入到logs/async_test.log" << std::endl;

    // 等待日志全部写入
    std::cout << "等待1秒，确保所有日志写入到磁盘..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * @brief 测试多线程日志功能
 * 演示在多线程环境下使用日志系统
 */
void testMultiThreadLogging()
{
    printTitle("多线程日志功能测试");

    auto logger = getLogger("ThreadTest");
    int threadCount = 4;
    int logsPerThread = 1000;

    std::cout << "启动" << threadCount << "个线程，每个线程写入"
              << logsPerThread << "条日志..." << std::endl;

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    // 创建多个线程并发写日志
    for (int t = 0; t < threadCount; ++t)
    {
        threads.emplace_back([t, logger, logsPerThread]()
                             {
            for (int i = 0; i < logsPerThread; ++i) {
                LOG_INFO(logger) << "线程" << t << "的第" << i << "条日志";
            } });
    }

    // 等待所有线程完成
    for (auto &t : threads)
    {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "多线程写入完成，总日志数: " << (threadCount * logsPerThread) << std::endl;
    std::cout << "总耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "平均每条日志写入时间: " << std::fixed << std::setprecision(3)
              << (duration.count() / static_cast<double>(threadCount * logsPerThread)) << "ms" << std::endl;
}

/**
 * @brief 演示实际应用场景中的日志使用
 */
void testRealWorldUsage()
{
    printTitle("实际应用场景示例");

    // 创建模块专用日志器
    auto userLogger = getLogger("UserService");
    auto dbLogger = getLogger("Database");

    // 模拟用户登录流程
    LOG_INFO(userLogger) << "用户开始登录流程";
    LOG_DEBUG(userLogger) << "接收到登录请求参数: username=admin";

    // 模拟数据库操作
    LOG_DEBUG(dbLogger) << "执行SQL: SELECT * FROM users WHERE username='admin'";
    LOG_INFO(dbLogger) << "数据库查询成功，返回1条记录";

    // 模拟密码验证
    LOG_DEBUG(userLogger) << "开始验证密码";
    LOG_WARN(userLogger) << "用户密码即将过期，还有5天";

    // 模拟登录成功
    LOG_INFO(userLogger) << "用户admin登录成功，IP: 192.168.1.100";

    // 模拟错误处理
    LOG_DEBUG(dbLogger) << "执行SQL: UPDATE users SET last_login=NOW() WHERE username='admin'";
    LOG_ERROR(dbLogger) << "数据库连接超时，无法更新最后登录时间";

    std::cout << "\n以上展示了在实际应用中如何使用不同级别的日志和多个日志器" << std::endl;
}

/**
 * @brief 主函数，运行所有测试
 */
int main(int argc, char *argv[])
{
    std::cout << "======== 日志系统功能测试 ========" << std::endl;

    // 确保日志目录存在
    system("mkdir -p ../logs");

    // 初始化LogManager
    LogManager::getInstance().init("../logs/test", 10 * 1024 * 1024, 1);

    // 运行测试
    testBasicLogging();
    // testFormatting();
    // testMultipleAppenders();
    // testFilters();
    // testAsyncLogging();
    // testMultiThreadLogging();
    // testRealWorldUsage();

    std::cout << "\n======== 所有测试完成 ========" << std::endl;
    return 0;
}