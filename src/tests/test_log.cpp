#include "Log.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <iomanip>
#include <functional>

/**
 * @brief 日志系统功能测试
 *
 * 本测试文件演示日志系统的核心功能，包括：
 * 1. 基本日志记录
 * 2. 日志级别控制
 * 3. 自定义格式化
 * 4. 多目标输出
 * 5. 日志过滤
 * 6. 异步日志和性能
 * 7. 自适应刷新和分级刷新
 * 8. 多线程安全
 */
int main(int argc, char *argv[])
{
    std::cout << "======== 日志系统功能测试 ========" << std::endl;

    // 确保日志目录存在
    system("mkdir -p ../logs");

    // 初始化LogManager - 这是配置日志系统的唯一入口
    initLogSystem("../logs/test", 10 * 1024 * 1024, 1);

    // 等待异步日志系统启动完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 1. 基本日志记录
    std::cout << "\n======== 1. 基本日志记录 ========" << std::endl;
    auto rootLogger = getLogger();
    LOG_INFO(rootLogger) << "这是来自root日志器的消息";

    auto moduleLogger = getLogger("MyModule");
    LOG_INFO(moduleLogger) << "这是来自MyModule日志器的消息";

    // 2. 日志级别控制
    std::cout << "\n======== 2. 日志级别控制 ========" << std::endl;
    std::cout << "设置MyModule日志级别为WARN，只有WARN及以上级别的日志会输出：" << std::endl;
    moduleLogger->setLevel(Level::WARN);

    LOG_DEBUG(moduleLogger) << "DEBUG级别日志 (MyModule) - 不会输出";
    LOG_INFO(moduleLogger) << "INFO级别日志 (MyModule) - 不会输出";
    LOG_WARN(moduleLogger) << "WARN级别日志 (MyModule) - 会输出";
    LOG_ERROR(moduleLogger) << "ERROR级别日志 (MyModule) - 会输出";

    // 恢复日志级别
    moduleLogger->setLevel(Level::DEBUG);
    std::cout << "MyModule日志级别已恢复为DEBUG" << std::endl;

    // // 3. 自定义格式化
    // std::cout << "\n======== 3. 自定义格式化 ========" << std::endl;
    // auto formatLogger = getLogger("FormatTest");

    // std::cout << "\n为FormatTest设置简洁格式：" << std::endl;
    // // 注意：自定义格式化会覆盖Appender的格式
    // formatLogger->setFormatter("%d{%H:%M:%S} [%p] %m%n");
    // LOG_INFO(formatLogger) << "这是简洁格式的日志";

    // // 清除自定义格式，恢复继承Appender的格式
    // formatLogger->setFormatter("");
    // LOG_INFO(formatLogger) << "已恢复为默认格式";

    // // 4. 多目标输出
    // std::cout << "\n======== 4. 多目标输出 ========" << std::endl;
    // std::cout << "root日志器默认输出到控制台和文件，MultiAppenderTest继承此行为" << std::endl;
    // auto multiLogger = getLogger("MultiAppenderTest");
    // LOG_INFO(multiLogger) << "这条日志会同时输出到控制台和文件";

    // // 5. 日志过滤
    // std::cout << "\n======== 5. 日志过滤 ========" << std::endl;
    // auto filterLogger = getLogger("FilterTest");

    // std::cout << "\n添加正则过滤器 (过滤包含 'password' 的日志):" << std::endl;
    // LogFilter::ptr regexFilter = std::make_shared<RegexFilter>("password", true);
    // filterLogger->addFilter(regexFilter);

    // LOG_INFO(filterLogger) << "普通日志 - 应正常输出";
    // LOG_WARN(filterLogger) << "含敏感词(password)的日志 - 应被过滤";
    // filterLogger->clearFilters();
    // LOG_INFO(filterLogger) << "过滤器已清除，含敏感词(password)的日志 - 现在可以输出";

    // // 6. 异步日志和性能
    // std::cout << "\n======== 6. 异步日志和性能 ========" << std::endl;
    // auto asyncLogger = getLogger("AsyncTest");

    // const int LOG_COUNT = 5000;
    // std::cout << "开始异步写入 " << LOG_COUNT << " 条日志..." << std::endl;
    // auto start = std::chrono::high_resolution_clock::now();
    // for (int i = 0; i < LOG_COUNT; ++i)
    // {
    //     LOG_INFO(asyncLogger) << "这是异步写入的第 " << i + 1 << " 条日志";
    // }
    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // std::cout << "写入 " << LOG_COUNT << " 条日志完成，耗时: " << duration.count() << " ms" << std::endl;

    // // 7. 自适应刷新和分级刷新
    // std::cout << "\n======== 7. 自适应刷新和分级刷新 ========" << std::endl;
    // auto adaptiveLogger = getLogger("AdaptiveTest");
    // std::cout << "ERROR和FATAL级别日志会立即刷新" << std::endl;
    // LOG_ERROR(adaptiveLogger) << "这是ERROR级别日志，将立即刷新到磁盘";

    // // 8. 多线程日志测试
    // std::cout << "\n======== 8. 多线程日志测试 ========" << std::endl;
    // int threadCount = 4;
    // int logsPerThread = 500;
    // std::cout << "启动 " << threadCount << " 个线程，每个线程写入 " << logsPerThread << " 条日志..." << std::endl;
    // std::vector<std::thread> threads;
    // start = std::chrono::high_resolution_clock::now();
    // for (int t = 0; t < threadCount; ++t)
    // {
    //     threads.emplace_back([t, logsPerThread]()
    //                          {
    //         auto threadLogger = getLogger("Thread" + std::to_string(t));
    //         for (int i = 0; i < logsPerThread; ++i) {
    //             LOG_INFO(threadLogger) << "线程 " << t << " 的第 " << i + 1 << " 条日志";
    //         } });
    // }
    // for (auto &t : threads)
    // {
    //     t.join();
    // }
    // end = std::chrono::high_resolution_clock::now();
    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // std::cout << "多线程写入完成，总耗时: " << duration.count() << " ms" << std::endl;

    std::cout << "\n等待2秒，确保所有异步日志都已落盘..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n======== 所有测试完成 ========" << std::endl;
    return 0;
}