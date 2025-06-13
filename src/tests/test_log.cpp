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
 * @brief 测试日志级别功能
 */
void testLogLevels()
{
    printTitle("测试日志级别功能");

    // 创建一个新的Logger，默认级别为DEBUG
    auto logger = getLogger("LevelTest");
    std::cout << "默认日志级别为DEBUG，所有级别的日志都会输出：" << std::endl;

    LOG_DEBUG(logger) << "这是DEBUG级别的日志";
    LOG_INFO(logger) << "这是INFO级别的日志";
    LOG_WARN(logger) << "这是WARN级别的日志";
    LOG_ERROR(logger) << "这是ERROR级别的日志";
    LOG_FATAL(logger) << "这是FATAL级别的日志";

    std::cout << "\n设置日志级别为WARN，只有WARN及以上级别的日志会输出：" << std::endl;
    logger->setLevel(Level::WARN);

    LOG_DEBUG(logger) << "这是DEBUG级别的日志，不会输出";
    LOG_INFO(logger) << "这是INFO级别的日志，不会输出";
    LOG_WARN(logger) << "这是WARN级别的日志，会输出";
    LOG_ERROR(logger) << "这是ERROR级别的日志，会输出";
    LOG_FATAL(logger) << "这是FATAL级别的日志，会输出";
}

/**
 * @brief 测试自定义格式化功能
 */
void testFormatting()
{
    printTitle("测试自定义格式化功能");

    auto logger = getLogger("FormatTest");

    // 原始格式
    std::cout << "默认日志格式：" << std::endl;
    LOG_INFO(logger) << "这是默认格式的日志";

    // 自定义格式1：简洁格式
    std::cout << "\n简洁格式：" << std::endl;
    logger->setFormatter("%d{%H:%M:%S} [%-5p] %m%n");
    LOG_INFO(logger) << "这是简洁格式的日志";

    // 自定义格式2：详细格式
    std::cout << "\n详细格式：" << std::endl;
    logger->setFormatter("%d{%Y-%m-%d %H:%M:%S.%f} [%p] [Thread:%t] %c %f:%l - %m%n");
    LOG_INFO(logger) << "这是详细格式的日志";

    // 自定义格式3：自定义样式
    std::cout << "\n自定义样式：" << std::endl;
    logger->setFormatter("* %d{%H:%M:%S} | %-5p | %m *%n");
    LOG_INFO(logger) << "这是自定义样式的日志";
}

/**
 * @brief 测试多输出目标功能
 */
void testMultipleAppenders()
{
    printTitle("测试多输出目标功能");

    auto logger = getLogger("MultiAppenderTest");
    logger->clearAppenders();

    // 添加控制台输出
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [ConsoleAppender] [%p] %m%n"));
    logger->addAppender(consoleAppender);

    // 添加文件输出
    LogAppenderPtr fileAppender(new FileLogAppender("../logs/multi_appender_test.log"));
    fileAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [FileAppender] [%p] %m%n"));
    logger->addAppender(fileAppender);

    // 输出日志，将同时写入控制台和文件
    LOG_INFO(logger) << "这条日志会同时输出到控制台和文件";
    LOG_ERROR(logger) << "错误日志同样会输出到两个目标";

    std::cout << "* 日志已同时输出到控制台和logs/multi_appender_test.log文件" << std::endl;
}

/**
 * @brief 测试过滤器功能
 */
void testFilters()
{
    printTitle("测试过滤器功能");

    auto logger = getLogger("FilterTest");
    logger->clearAppenders();

    // 添加控制台输出
    LogAppenderPtr consoleAppender(new StdoutLogAppender());
    consoleAppender->setFormatter(std::make_shared<LogFormatter>(
        "%d{%H:%M:%S} [%p] %m%n"));
    logger->addAppender(consoleAppender);

    // 1. 级别过滤器 - 过滤DEBUG级别的日志
    std::cout << "添加级别过滤器，过滤掉DEBUG级别的日志：" << std::endl;
    LogFilter::ptr levelFilter = std::make_shared<LevelFilter>(Level::INFO);
    logger->addFilter(levelFilter);

    LOG_DEBUG(logger) << "这是DEBUG日志，应该被过滤掉";
    LOG_INFO(logger) << "这是INFO日志，应该正常输出";

    // 清除过滤器
    logger->clearFilters();

    // 2. 正则表达式过滤器 - 过滤包含"password"的日志
    std::cout << "\n添加正则过滤器，过滤包含'password'的日志：" << std::endl;
    LogFilter::ptr regexFilter = std::make_shared<RegexFilter>("password", true);
    logger->addFilter(regexFilter);

    LOG_INFO(logger) << "这是普通日志，应该正常输出";
    LOG_INFO(logger) << "用户密码(password)是123456，应该被过滤掉";

    // 清除过滤器
    logger->clearFilters();

    // 3. 自定义函数过滤器 - 过滤长度超过20的日志
    std::cout << "\n添加函数过滤器，过滤长度超过20的日志：" << std::endl;
    LogFilter::ptr funcFilter = std::make_shared<FunctionFilter>(
        [](LogEvent::ptr event)
        {
            return event->getStringStream().str().length() > 20;
        });
    logger->addFilter(funcFilter);

    LOG_INFO(logger) << "短日志";
    LOG_INFO(logger) << "这是一条非常长的日志内容，应该被过滤掉，因为长度超过了20个字符";

    // 清除过滤器
    logger->clearFilters();

    // 4. 复合过滤器 - 组合多个条件
    std::cout << "\n添加复合过滤器(OR关系)，过滤DEBUG级别或包含'error'的日志：" << std::endl;
    CompositeFilter *compositeFilter = new CompositeFilter(false); // false表示OR关系
    compositeFilter->addFilter(std::make_shared<LevelFilter>(Level::INFO));
    compositeFilter->addFilter(std::make_shared<RegexFilter>("error", false)); // false表示匹配则保留
    logger->addFilter(LogFilter::ptr(compositeFilter));

    LOG_DEBUG(logger) << "DEBUG级别，应被过滤";
    LOG_INFO(logger) << "INFO但不含error，应该输出";
    LOG_WARN(logger) << "这是一条包含error关键字的日志，因为设置的是保留匹配的，所以应该输出";
}

/**
 * @brief 测试异步日志功能
 */
void testAsyncLogging()
{
    printTitle("测试异步日志功能");

    // 确保日志目录存在
    system("mkdir -p ../logs");

    // 创建全局变量，供静态函数使用
    static std::unique_ptr<AsyncLogging> g_testAsyncLogger =
        std::make_unique<AsyncLogging>("../logs/async_test", 1024 * 1024, 3);
    g_testAsyncLogger->start();

    // 保存原来的异步输出函数指针
    auto oldAsyncOutputFunc = g_asyncOutputFunc;

    // 设置新的异步输出函数，使用静态函数而非捕获lambda
    g_asyncOutputFunc = [](const char *msg, int len)
    {
        if (g_testAsyncLogger)
        {
            g_testAsyncLogger->append(msg, len);
        }
    };

    auto logger = getLogger("AsyncTest");
    logger->clearAppenders();

    // 添加文件输出器
    LogAppenderPtr fileAppender(new FileLogAppender("../logs/async_test.log"));
    logger->addAppender(fileAppender);

    std::cout << "开始异步写入10000条日志..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    // 快速写入10000条日志
    for (int i = 0; i < 10000; ++i)
    {
        LOG_INFO(logger) << "这是异步写入的第 " << i << " 条日志";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "写入10000条日志完成，耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "平均每条日志写入时间: " << std::fixed << std::setprecision(3)
              << (duration.count() / 10000.0) << "ms" << std::endl;
    std::cout << "日志已异步写入到logs/async_test.log" << std::endl;

    // 等待日志全部写入
    std::cout << "等待3秒，确保所有日志写入到磁盘..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 恢复原来的异步输出函数
    g_asyncOutputFunc = oldAsyncOutputFunc;

    // 停止异步日志线程
    g_testAsyncLogger->stop();
    g_testAsyncLogger.reset(); // 释放资源
}

/**
 * @brief 测试LogManager功能
 */
void testLogManager()
{
    printTitle("测试LogManager功能");

    // 初始化日志系统
    LogManager::getInstance().init("../logs", 10 * 1024 * 1024, 3);

    // 获取不同名称的日志器
    auto rootLogger = getLogger();
    auto moduleLogger = getLogger("Module1");
    auto subModuleLogger = getLogger("Module1.SubModule");

    // 修改级别
    moduleLogger->setLevel(Level::INFO);

    // 通过LogManager记录日志
    LOG_INFO(rootLogger) << "来自root日志器的信息";
    LOG_INFO(moduleLogger) << "来自Module1日志器的信息";
    LOG_INFO(subModuleLogger) << "来自SubModule日志器的信息";

    // 展示继承关系
    std::cout << "LogManager会自动建立Logger的继承体系：" << std::endl;
    std::cout << "- root日志器(默认)" << std::endl;
    std::cout << "- Module1日志器(继承root配置)" << std::endl;
    std::cout << "- Module1.SubModule日志器(继承root配置)" << std::endl;
    std::cout << "\n以上日志已输出到控制台并异步写入到logs/manager_test.log" << std::endl;
}

/**
 * @brief 测试多线程日志功能
 */
void testMultiThreadLogging()
{
    printTitle("测试多线程日志功能");

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
 * @brief 主函数，运行所有测试
 */
int main(int argc, char *argv[])
{
    std::cout << "======== 日志系统功能测试 ========" << std::endl;

    // 确保日志目录存在
    system("mkdir -p ../logs");

    // 初始化LogManager
    LogManager::getInstance().init();

    // 运行测试
    testLogLevels();
    testFormatting();
    testMultipleAppenders();
    testFilters();
    testAsyncLogging();
    testLogManager();
    testMultiThreadLogging();

    std::cout << "\n======== 所有测试完成 ========" << std::endl;
    return 0;
}