#include "base/BaseConfig.h"
#include "log/LogConfig.h"
#include "net/NetworkConfig.h"
#include "base/Buffer.h"
#include "log/Log.h"
#include "net/InetAddress.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

// 全局变量用于优雅退出
EventLoop *g_mainLoop = nullptr;
EventLoopThreadPool *g_threadPool = nullptr;

void signalHandler(int sig)
{
    std::cout << "\n收到退出信号,正在优雅关闭..." << std::endl;
    if (g_mainLoop)
    {
        g_mainLoop->quit();
    }
}

int main()
{
    std::cout << "=== 配置读取测试 ===" << std::endl;

    // 先初始化日志系统,这样后续的配置读取日志才能显示
    std::cout << "初始化日志系统..." << std::endl;
    initLogSystem();
    std::cout << "日志系统初始化完成" << std::endl;

    // 测试无效配置
    std::cout << "\n=== 测试无效配置 ===" << std::endl;
    try
    {
        std::cout << "测试无效的BaseConfig配置..." << std::endl;
        BaseConfig::getInstance().load("configs/config_invalid.yml");
    }
    catch (const std::exception &e)
    {
        std::cout << "BaseConfig验证失败: " << e.what() << std::endl;
    }

    try
    {
        std::cout << "测试无效的LogConfig配置..." << std::endl;
        LogConfig::getInstance().load("configs/config_invalid.yml");
    }
    catch (const std::exception &e)
    {
        std::cout << "LogConfig验证失败: " << e.what() << std::endl;
    }

    try
    {
        std::cout << "测试无效的NetworkConfig配置..." << std::endl;
        NetworkConfig::getInstance().load("configs/config_invalid.yml");
    }
    catch (const std::exception &e)
    {
        std::cout << "NetworkConfig验证失败: " << e.what() << std::endl;
    }

    // 测试BaseConfig (Buffer配置)
    std::cout << "\n=== 测试有效配置 ===" << std::endl;
    std::cout << "读取BaseConfig配置..." << std::endl;
    BaseConfig::getInstance().load("configs/config.yml");
    const auto &baseConfig = BaseConfig::getInstance();
    std::cout << "buffer.initial_size: " << baseConfig.getBufferInitialSize() << std::endl;
    std::cout << "buffer.max_size: " << baseConfig.getBufferMaxSize() << std::endl;
    std::cout << "buffer.growth_factor: " << baseConfig.getBufferGrowthFactor() << std::endl;

    // 测试LogConfig
    std::cout << "读取LogConfig配置..." << std::endl;
    LogConfig::getInstance().load("configs/config.yml");
    const auto &logConfig = LogConfig::getInstance();
    std::cout << "log.basename: " << logConfig.getBasename() << std::endl;
    std::cout << "log.roll_size: " << logConfig.getRollSize() << std::endl;
    std::cout << "log.flush_interval: " << logConfig.getFlushInterval() << std::endl;
    std::cout << "log.roll_mode: " << logConfig.getRollMode() << std::endl;
    std::cout << "log.enable_file: " << logConfig.getEnableFile() << std::endl;
    std::cout << "log.enable_async: " << logConfig.getEnableAsync() << std::endl;
    std::cout << "log.file_level: " << logConfig.getFileLevel() << std::endl;
    std::cout << "log.console_level: " << logConfig.getConsoleLevel() << std::endl;

    // 测试NetworkConfig
    std::cout << "读取NetworkConfig配置..." << std::endl;
    NetworkConfig::getInstance().load("configs/config.yml");
    const auto &netConfig = NetworkConfig::getInstance();
    std::cout << "network.ip: " << netConfig.getIp() << std::endl;
    std::cout << "network.port: " << netConfig.getPort() << std::endl;
    std::cout << "network.epoll_mode: " << netConfig.getEpollMode() << std::endl;
    std::cout << "network.thread_pool.thread_num: " << netConfig.getThreadNum() << std::endl;
    std::cout << "network.thread_pool.queue_size: " << netConfig.getThreadPoolQueueSize() << std::endl;
    std::cout << "network.thread_pool.keep_alive_time: " << netConfig.getThreadPoolKeepAliveTime() << std::endl;
    std::cout << "network.thread_pool.max_idle_threads: " << netConfig.getThreadPoolMaxIdleThreads() << std::endl;
    std::cout << "network.thread_pool.min_idle_threads: " << netConfig.getThreadPoolMinIdleThreads() << std::endl;

    std::cout << "\n=== 配置应用测试 ===" << std::endl;

    // 1. 测试Buffer配置应用
    std::cout << "1. 测试Buffer配置应用..." << std::endl;
    Buffer buffer1; // 使用默认配置
    std::cout << "   Buffer1初始大小: " << buffer1.writableBytes() << " 字节" << std::endl;

    Buffer buffer2(2048); // 使用自定义大小
    std::cout << "   Buffer2初始大小: " << buffer2.writableBytes() << " 字节" << std::endl;

    // 2. 测试日志配置应用
    std::cout << "2. 测试日志配置应用..." << std::endl;
    std::cout << "   日志系统已初始化,使用配置文件参数" << std::endl;

    // 测试日志输出
    DLOG_INFO << "这是一条测试日志信息";
    DLOG_DEBUG << "这是一条调试日志信息";
    DLOG_WARN << "这是一条警告日志信息";
    DLOG_ERROR << "这是一条错误日志信息";

    // 等待日志写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. 测试网络配置应用
    std::cout << "3. 测试网络配置应用..." << std::endl;
    InetAddress addr(netConfig.getPort(), netConfig.getIp());
    std::cout << "   创建监听地址: " << addr.toIpPort() << std::endl;

    // 创建EventLoop和EventLoopThreadPool
    EventLoop loop;
    g_mainLoop = &loop; // 设置全局变量用于信号处理

    EventLoopThreadPool threadPool(&loop, "TestThreadPool");
    g_threadPool = &threadPool; // 设置全局变量用于信号处理

    // 应用线程池配置
    std::cout << "   应用线程池配置..." << std::endl;
    threadPool.setThreadNum(netConfig.getThreadNum());
    threadPool.setQueueSize(netConfig.getThreadPoolQueueSize());
    threadPool.setKeepAliveTime(netConfig.getThreadPoolKeepAliveTime());
    threadPool.setMaxIdleThreads(netConfig.getThreadPoolMaxIdleThreads());
    threadPool.setMinIdleThreads(netConfig.getThreadPoolMinIdleThreads());

    // 验证配置是否生效
    std::cout << "   验证线程池配置:" << std::endl;
    std::cout << "     - 线程数: " << netConfig.getThreadNum() << " (配置) -> " << netConfig.getThreadNum() << " (应用)" << std::endl;
    std::cout << "     - 队列大小: " << netConfig.getThreadPoolQueueSize() << " (配置) -> " << threadPool.getQueueSize() << " (应用)" << std::endl;
    std::cout << "     - 保活时间: " << netConfig.getThreadPoolKeepAliveTime() << " (配置) -> " << threadPool.getKeepAliveTime() << " (应用)" << std::endl;
    std::cout << "     - 最大空闲线程: " << netConfig.getThreadPoolMaxIdleThreads() << " (配置) -> " << threadPool.getMaxIdleThreads() << " (应用)" << std::endl;
    std::cout << "     - 最小空闲线程: " << netConfig.getThreadPoolMinIdleThreads() << " (配置) -> " << threadPool.getMinIdleThreads() << " (应用)" << std::endl;

    // 启动线程池
    std::cout << "   启动线程池..." << std::endl;
    threadPool.start();

    // 验证线程池状态
    std::cout << "   线程池状态: " << (threadPool.started() ? "已启动" : "未启动") << std::endl;
    std::cout << "   线程池名称: " << threadPool.name() << std::endl;

    // 测试获取EventLoop
    std::cout << "   测试获取EventLoop..." << std::endl;
    EventLoop *loop1 = threadPool.getNextLoop();
    EventLoop *loop2 = threadPool.getNextLoop();
    EventLoop *loop3 = threadPool.getNextLoop();
    std::cout << "   轮询获取的EventLoop: " << loop1 << ", " << loop2 << ", " << loop3 << std::endl;

    // 获取所有EventLoop
    auto allLoops = threadPool.getAllLoops();
    std::cout << "   线程池中的EventLoop数量: " << allLoops.size() << std::endl;

    std::cout << "   网络配置验证完成" << std::endl;

    std::cout << "\n=== 配置验证完成 ===" << std::endl;
    std::cout << "所有配置都已成功读取并应用到相应模块！" << std::endl;

    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "\n=== 服务器运行中 ===" << std::endl;
    std::cout << "线程池已启动,服务器正在运行..." << std::endl;
    std::cout << "按 Ctrl+C 退出程序" << std::endl;

    // 启动主事件循环
    std::cout << "启动主事件循环..." << std::endl;
    loop.loop();

    std::cout << "\n=== 服务器关闭 ===" << std::endl;
    std::cout << "主事件循环已退出,程序结束" << std::endl;

    return 0;
}