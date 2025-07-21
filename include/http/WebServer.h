#pragma once
#include "HttpServer.h"
#include "net/EventLoop.h"
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <functional>
#include <vector>
#include "log/Log.h"
#include <unordered_map>
#include <mutex>

class ThreadPool; // 业务线程池前置声明

/**
 * @brief WebServer 主服务器类
 *
 * 负责初始化日志系统、加载配置、创建主事件循环、HTTP服务器、业务线程池，
 * 并统一管理服务器的启动、停止、优雅关闭、HTTP请求分发等核心流程。
 * 支持静态资源与动态接口的路由分发。
 */
class WebServer
{
public:
    /**
     * @brief 构造函数，完成日志、配置、主循环、HTTP服务器、线程池等初始化
     */
    WebServer();
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
     * @brief 静态入口，创建 WebServer 并注册信号处理，自动优雅关闭
     */
    static void run();
    /**
     * @brief 信号处理函数，收到 SIGINT/SIGTERM 时优雅关闭服务器
     * @param signo 信号编号
     */
    static void shutdown_handler(int signo);
    /**
     * @brief 注册路由
     * @param path 路径（如 /api/hello）
     * @param cb   处理回调
     */
    void addRoute(const std::string &path, const HttpServer::HttpCallback &cb);
    /**
     * @brief 设置未命中路由时的回调（可选）
     */
    void setNotFoundHandler(const HttpServer::HttpCallback &cb);

private:
    /**
     * @brief 初始化日志系统(可扩展为异步/多级日志等)
     */
    void initLog();
    /**
     * @brief 加载服务器配置(如IP、端口、线程数等)
     */
    void initConfig();
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
    /**
     * @brief 处理静态资源请求(如 html/css/js 等)
     * @param req HTTP请求对象
     * @param resp HTTP响应对象
     */
    void handleStatic(const HttpRequest &req, HttpResponse *resp);
    /**
     * @brief 处理动态接口请求(如 /api/ 路径)
     * @param req HTTP请求对象
     * @param resp HTTP响应对象
     */
    void handleDynamic(const HttpRequest &req, HttpResponse *resp);

    std::unique_ptr<EventLoop> mainLoop_;                              // 主事件循环对象，负责 IO 事件分发
    std::unique_ptr<HttpServer> server_;                               // HTTP服务器对象，负责 TCP 连接与 HTTP 协议处理
    std::unique_ptr<ThreadPool> businessPool_;                         // 业务线程池，处理耗时任务(可扩展)
    std::shared_ptr<LogManager> logManager_;                           // 日志管理器，保证日志系统生命周期覆盖 WebServer
    std::atomic_bool running_;                                         // 运行状态标志，线程安全
    std::unordered_map<std::string, HttpServer::HttpCallback> router_; // 路由表
    std::mutex routerMutex_;                                           // 路由表线程安全
    HttpServer::HttpCallback notFoundHandler_;                         // 404处理回调
};