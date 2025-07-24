#pragma once
#include "http/HttpServer.h"
#include "net/EventLoop.h"
#include "http/Router.h"
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <functional>
#include <vector>
#include "log/Log.h"
#include <unordered_map>
#include <mutex>

class ThreadPool;
class ConfigManager;
class NetworkConfig;

/**
 * @brief WebServer 主服务器类
 */
class WebServer
{
public:
    /**
     * @brief 构造函数，完成所有模块的初始化
     * @param configManager 配置管理器的引用
     */
    explicit WebServer(ConfigManager &configManager);

    /**
     * @brief 析构函数，自动优雅关闭服务器
     */
    ~WebServer();

    /**
     * @brief 启动服务器(包括日志、线程池、网络服务、主事件循环)
     */
    void start();
    /**
     * @brief 停止服务器，优雅关闭所有资源
     */
    void stop();
    /**
     * @brief 获取路由器的引用，用于注册路由和中间件
     * @return Router&
     */
    Router &getRouter() { return router_; }

    /**
     * @brief 注册路由
     */
    void registerRoutes();

private:
    /**
     * @brief 初始化 HTTP 回调，将请求分发到 onHttpRequest
     */
    void initCallbacks();
    /**
     * @brief HTTP请求统一入口，根据 path 路由到静态或动态处理
     * @param req HTTP请求对象
     * @param resp HTTP响应对象
     */
    void onHttpRequest(const HttpRequest &req, HttpResponse *resp);

    ConfigManager &configManager_;                 // 保存配置管理器的引用
    std::shared_ptr<NetworkConfig> networkConfig_; // 保存网络配置

    std::unique_ptr<EventLoop> mainLoop_;      // 主事件循环对象，负责 IO 事件分发
    std::unique_ptr<HttpServer> server_;       // HTTP服务器对象，负责 TCP 连接与 HTTP 协议处理
    std::unique_ptr<ThreadPool> businessPool_; // 业务线程池，处理耗时任务(可扩展)
    std::shared_ptr<LogManager> logManager_;   // 日志管理器，保证日志系统生命周期覆盖 WebServer
    std::atomic_bool running_;                 // 运行状态标志，线程安全
    Router router_;                            // 路由器，负责路由和中间件管理
};

// JWT认证检查函数声明
bool checkAuth(const HttpRequest &req, int &user_id);