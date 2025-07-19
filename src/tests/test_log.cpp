#include "log/Log.h"
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
 * 9. 综合滚动策略（按大小和时间）
 */
int main(int argc, char *argv[])
{
    std::cout << "======== 日志系统功能测试 ========" << std::endl;

    // 确保日志目录存在
    system("mkdir -p logs");

    // 初始化LogManager - 这是配置日志系统的唯一入口
    // 使用综合滚动策略：按大小和每小时滚动
    // 为了测试方便，设置较小的滚动大小（1MB）
    initLogSystem();

    // 等待异步日志系统启动完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /*
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

    // 3. 自定义格式化
    std::cout << "\n======== 3. 自定义格式化 ========" << std::endl;
    auto formatLogger = getLogger("FormatTest");

    std::cout << "\n为FormatTest设置简洁格式：" << std::endl;
    // 注意：自定义格式化会覆盖Appender的格式
    // 创建一个简洁格式的格式化器
    LogFormatter::ptr formatter = std::make_shared<LogFormatter>("%d{%H:%M:%S} [%p] %m%n");
    formatLogger->setFormatter(formatter);
    LOG_INFO(formatLogger) << "这是简洁格式的日志";

    // 清除自定义格式，恢复继承Appender的格式
    formatLogger->setFormatter(LogFormatter::ptr());
    LOG_INFO(formatLogger) << "已恢复为默认格式";

    // 4. 多目标输出
    std::cout << "\n======== 4. 多目标输出 ========" << std::endl;
    std::cout << "root日志器默认输出到控制台和文件，MultiAppenderTest继承此行为" << std::endl;
    auto multiLogger = getLogger("MultiAppenderTest");
    LOG_INFO(multiLogger) << "这条日志会同时输出到控制台和文件";

    // 5. 日志过滤
    std::cout << "\n======== 5. 日志过滤 ========" << std::endl;
    auto filterLogger = getLogger("FilterTest");

    std::cout << "\n添加关键字过滤器 (过滤包含 'password' 的日志):" << std::endl;
    auto regexFilter = std::make_shared<RegexFilter>("password", true);
    filterLogger->addFilter(regexFilter);

    LOG_INFO(filterLogger) << "普通日志 - 应正常输出";
    LOG_WARN(filterLogger) << "含敏感词(password)的日志 - 应被过滤";
    filterLogger->clearFilters();
    LOG_INFO(filterLogger) << "过滤器已清除，含敏感词(password)的日志 - 现在可以输出";

    // 6. 异步日志和性能
    std::cout << "\n======== 6. 异步日志和性能 ========" << std::endl;
    auto asyncLogger = getLogger("AsyncTest");

    const int LOG_COUNT = 5000;
    std::cout << "开始异步写入 " << LOG_COUNT << " 条日志..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < LOG_COUNT; ++i)
    {
        LOG_INFO(asyncLogger) << "这是异步写入的第 " << i + 1 << " 条日志";
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "写入 " << LOG_COUNT << " 条日志完成，耗时: " << duration.count() << " ms" << std::endl;

    // 7. 自适应刷新和分级刷新
    std::cout << "\n======== 7. 自适应刷新和分级刷新 ========" << std::endl;
    auto adaptiveLogger = getLogger("AdaptiveTest");
    std::cout << "ERROR和FATAL级别日志会立即刷新" << std::endl;
    LOG_ERROR(adaptiveLogger) << "这是ERROR级别日志，将立即刷新到磁盘";

    // 8. 多线程日志测试
    std::cout << "\n======== 8. 多线程日志测试 ========" << std::endl;
    int threadCount = 4;
    int logsPerThread = 500;
    std::cout << "启动 " << threadCount << " 个线程，每个线程写入 " << logsPerThread << " 条日志..." << std::endl;
    std::vector<std::thread> threads;
    start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < threadCount; ++t)
    {
        threads.emplace_back([t, logsPerThread]()
                             {
            auto threadLogger = getLogger("Thread" + std::to_string(t));
            for (int i = 0; i < logsPerThread; ++i) {
                LOG_INFO(threadLogger) << "线程 " << t << " 的第 " << i + 1 << " 条日志";
            } });
    }
    for (auto &t : threads)
    {
        t.join();
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "多线程写入完成，总耗时: " << duration.count() << " ms" << std::endl;

    // 9. 测试综合滚动策略
    std::cout << "\n======== 9. 综合滚动策略 ========" << std::endl;
    auto rollLogger = getLogger("RollTest");

    // 当前使用的滚动策略
    std::string rollModeStr;
    switch (LogFile::RollMode::SIZE_HOURLY)
    {
    case LogFile::RollMode::SIZE:
        rollModeStr = "按大小滚动";
        break;
    case LogFile::RollMode::DAILY:
        rollModeStr = "每天滚动";
        break;
    case LogFile::RollMode::HOURLY:
        rollModeStr = "每小时滚动";
        break;
    case LogFile::RollMode::MINUTELY:
        rollModeStr = "每分钟滚动";
        break;
    case LogFile::RollMode::SIZE_DAILY:
        rollModeStr = "按大小和每天滚动";
        break;
    case LogFile::RollMode::SIZE_HOURLY:
        rollModeStr = "按大小和每小时滚动";
        break;
    case LogFile::RollMode::SIZE_MINUTELY:
        rollModeStr = "按大小和每分钟滚动";
        break;
    default:
        rollModeStr = "未知模式";
        break;
    }

    std::cout << "当前使用的滚动策略: " << rollModeStr << std::endl;
    std::cout << "滚动大小阈值: 1MB" << std::endl;

    // 写入一些大日志，测试按大小滚动
    std::string bigData(10 * 1024, 'X'); // 10KB的数据
    std::cout << "写入100条大日志(每条约10KB)，测试按大小滚动..." << std::endl;
    for (int i = 0; i < 100; ++i)
    {
        LOG_INFO(rollLogger) << "大日志 #" << i + 1 << " " << bigData;
    }

    std::cout << "日志已写入，请检查日志文件是否已滚动" << std::endl;
    std::cout << "日志文件位置: logs/server*" << std::endl;

    // 等待所有日志写入完成
    std::cout << "\n等待2秒，确保所有异步日志都已落盘..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    */

    // 测试动态修改滚动策略
    std::cout << "\n======== 10. 动态修改滚动策略 ========" << std::endl;
    std::cout << "将滚动策略修改为: 按大小和每天滚动" << std::endl;
    setLogRollMode(LogFile::RollMode::SIZE_DAILY);

    // 为了让重新初始化的效果可见，我们在这里也打一条日志
    auto rootLogger = getLogger();
    LOG_INFO(rootLogger) << "滚动策略已修改为: 按大小和每天滚动";

    std::cout << "\n======== 所有测试完成 ========" << std::endl;
    return 0;
}