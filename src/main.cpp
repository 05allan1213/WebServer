#include "http/WebServer.h"
#include "base/ConfigManager.h"
#include "log/LogManager.h"
#include "db/DBConnectionPool.h"
#include <iostream>

int main()
{
    // 1. 引导日志，这是唯一在服务启动前需要的
    initDefaultLogger();

    try
    {
        // 2. 在一个独立的作用域中运行所有服务
        {
            // 获取单例实例
            auto logManager = LogManager::getInstance();
            auto &configManager = ConfigManager::getInstance();
            auto dbConnectionPool = DBConnectionPool::getInstance();

            // 加载配置
            configManager.load("configs/config.yml", 5);

            // 检查配置有效性
            if (!configManager.getNetworkConfig() || !configManager.getLogConfig() ||
                !configManager.getDBConfig() || !configManager.getBaseConfig())
            {
                DLOG_FATAL << "[Main] 核心配置加载失败，服务器无法启动。";
                // 在退出前尝试关闭已经启动的服务
                logManager->shutdown();
                return 1;
            }

            // 使用配置初始化服务
            initLogSystem();
            dbConnectionPool->init(*configManager.getDBConfig());

            // 创建并运行 WebServer
            WebServer server(configManager);
            DLOG_INFO << "[Main] WebServer 启动中...";
            server.start(); // 阻塞，直到收到 Ctrl+C
            DLOG_INFO << "[Main] WebServer 的事件循环已停止。";

            // 当 server.start() 返回后，意味着网络服务已停止
            // 现在，我们按依赖关系的逆序关闭所有后台服务

            DLOG_INFO << "[Main] 正在关闭核心服务...";

            // 依赖别人的先关闭
            // ConfigManager 依赖日志系统
            configManager.shutdown();
            // DBConnectionPool 依赖日志系统
            dbConnectionPool->shutdown();

            // 日志系统是所有系统的基础，必须最后一个关闭
            logManager->shutdown();

        } // 所有局部变量（包括 server 和单例的智能指针/指针）在此处离开作用域
    }
    catch (const std::exception &e)
    {
        // 确保即使有异常，也能尝试记录日志并关闭
        DLOG_FATAL << "[Main] 出现异常: " << e.what();
        LogManager::getInstance()->shutdown();
        return 1;
    }

    std::cout << "[Main] 应用程序已完全关闭。" << std::endl;
    return 0;
}