#include "WebServer.h"
#include "StaticFileHandler.h"
#include "net/NetworkConfig.h"
#include "log/Log.h"
#include "log/LogManager.h"
#include <csignal>
#include <memory>
#include <fstream>
#include <sstream>
#include "db/DBConfig.h"
#include "db/DBConnectionPool.h"

/**
 * @brief 业务线程池简单占位实现(可扩展为真正的线程池)
 */
class ThreadPool
{
public:
    /// 启动线程池
    void start() {}
    /// 停止线程池
    void stop() {}
};

/// WebServer 全局唯一指针，用于信号优雅关闭
static std::unique_ptr<WebServer> g_server;

/**
 * @brief 信号处理函数，收到 SIGINT/SIGTERM 时优雅关闭服务器
 * @param signo 信号编号
 */
void WebServer::shutdown_handler(int signo)
{
    DLOG_INFO << "[WebServer] 收到信号 " << signo << ", 正在优雅关闭...";
    if (g_server)
        g_server->stop();
}

/**
 * @brief 服务器主入口，创建 WebServer 并注册信号处理，自动优雅关闭
 */
void WebServer::run()
{
    // 1. 加载数据库配置
    DBConfig dbConfig("configs/config.yml");
    if (!dbConfig.isValid())
    {
        DLOG_FATAL << "[WebServer] 数据库配置无效: " << dbConfig.getErrorMsg();
        throw std::runtime_error("[WebServer] 数据库配置无效: " + dbConfig.getErrorMsg());
    }
    DLOG_INFO << "[WebServer] 数据库配置加载成功";
    // 2. 初始化数据库连接池
    DBConnectionPool::getInstance()->init(dbConfig);
    DLOG_INFO << "[WebServer] 数据库连接池初始化完成";
    // 3. 启动WebServer
    g_server = std::make_unique<WebServer>();
    std::signal(SIGINT, shutdown_handler);
    std::signal(SIGTERM, shutdown_handler);
    g_server->start();
}

/**
 * @brief 构造函数，完成日志、配置、主循环、HTTP服务器、线程池等初始化
 */
WebServer::WebServer() : running_(false)
{
    logManager_ = LogManager::getInstance();
    initLog();
    initConfig();
    mainLoop_ = std::make_unique<EventLoop>(NetworkConfig::getInstance().getEpollMode());
    const auto &netConfig = NetworkConfig::getInstance();
    InetAddress addr(netConfig.getPort(), netConfig.getIp());
    server_ = std::make_unique<HttpServer>(mainLoop_.get(), addr, "WebServer-01", netConfig.getThreadNum());
    businessPool_ = std::make_unique<ThreadPool>();
    initCallbacks();
}

/**
 * @brief 析构函数，自动优雅关闭服务器
 */
WebServer::~WebServer()
{
    stop();
}

/**
 * @brief 初始化日志系统(可扩展为异步/多级日志等)
 */
void WebServer::initLog()
{
    DLOG_INFO << "[WebServer] 日志系统初始化";
}

/**
 * @brief 加载服务器配置(如IP、端口、线程数等)
 */
void WebServer::initConfig()
{
    NetworkConfig::getInstance().load("configs/config.yml");
    DLOG_INFO << "[WebServer] 配置加载完成";
}

/**
 * @brief 初始化 HTTP 回调，将请求分发到 onHttpRequest
 */
void WebServer::initCallbacks()
{
    server_->setHttpCallback(std::bind(&WebServer::onHttpRequest, this, std::placeholders::_1, std::placeholders::_2));
}

/**
 * @brief HTTP请求统一入口，根据 path 路由到静态或动态处理
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
void WebServer::onHttpRequest(const HttpRequest &req, HttpResponse *resp)
{
    // 路由分发：以 /api/ 开头的为动态接口，其余为静态资源
    if (req.getPath().find("/api/") == 0)
    {
        handleDynamic(req, resp);
    }
    else
    {
        handleStatic(req, resp);
    }
}

/**
 * @brief 处理静态资源请求(如 html/css/js 等)
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
void WebServer::handleStatic(const HttpRequest &req, HttpResponse *resp)
{
    if (StaticFileHandler::handle(req, resp))
    {
        DLOG_INFO << "[WebServer] 静态资源处理成功: " << req.getPath();
    }
    else
    {
        DLOG_WARN << "[WebServer] 静态资源未找到: " << req.getPath();
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/html");
        std::ifstream ifs("web_static/404.html");
        std::stringstream buffer;
        if (ifs)
        {
            buffer << ifs.rdbuf();
            resp->setBody(buffer.str());
        }
        else
        {
            resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        }
    }
}

/**
 * @brief 处理动态接口请求(如 /api/ 路径)
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
void WebServer::handleDynamic(const HttpRequest &req, HttpResponse *resp)
{
    // 业务逻辑处理(可扩展为线程池任务)
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("application/json");
    resp->setBody("{\"msg\":\"动态接口响应\"}");
}

/**
 * @brief 启动服务器(包括日志、线程池、网络服务、主事件循环)
 */
void WebServer::start()
{
    if (running_.exchange(true))
        return;
    DLOG_INFO << "[WebServer] 启动...";
    // 启动业务线程池
    businessPool_->start();
    server_->start();
    mainLoop_->loop();
    DLOG_INFO << "[WebServer] 已停止.";
}

/**
 * @brief 停止服务器，优雅关闭所有资源
 */
void WebServer::stop()
{
    if (!running_.exchange(false))
        return;
    DLOG_INFO << "[WebServer] 停止中...";
    businessPool_->stop();
    mainLoop_->quit();
}