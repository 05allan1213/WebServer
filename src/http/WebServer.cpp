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
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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
    /**
     * @brief 初始化数据库配置和连接池，必须在 TcpServer 创建前完成
     *
     * 1. 加载数据库配置文件，校验有效性。
     * 2. 初始化数据库连接池，创建初始连接。
     * 这样可确保在第一个 HTTP 请求到来时，连接池已就绪。
     *
     * @throws std::runtime_error 如果数据库配置无效，直接终止服务启动。
     */
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
    /**
     * @brief 初始化数据库配置和连接池，必须在 TcpServer 创建前完成
     *
     * 1. 加载数据库配置文件，校验有效性。
     * 2. 初始化数据库连接池，创建初始连接。
     * 这样可确保在第一个 HTTP 请求到来时，连接池已就绪。
     *
     * @throws std::runtime_error 如果数据库配置无效，直接终止服务启动。
     */
    DBConfig dbConfig("configs/config.yml");
    if (!dbConfig.isValid())
    {
        DLOG_FATAL << "[WebServer] 数据库配置无效: " << dbConfig.getErrorMsg();
        throw std::runtime_error("[WebServer] 数据库配置无效: " + dbConfig.getErrorMsg());
    }
    DLOG_INFO << "[WebServer] 数据库配置加载成功";
    DBConnectionPool::getInstance()->init(dbConfig);
    DLOG_INFO << "[WebServer] 数据库连接池初始化完成";
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

void WebServer::addRoute(const std::string &path, const HttpServer::HttpCallback &cb)
{
    std::lock_guard<std::mutex> lock(routerMutex_);
    router_[path] = cb;
}

void WebServer::setNotFoundHandler(const HttpServer::HttpCallback &cb)
{
    std::lock_guard<std::mutex> lock(routerMutex_);
    notFoundHandler_ = cb;
}

/**
 * @brief HTTP请求统一入口，根据 path 路由到静态或动态处理
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
void WebServer::onHttpRequest(const HttpRequest &req, HttpResponse *resp)
{
    // 路由分发：优先查找路由表
    {
        std::lock_guard<std::mutex> lock(routerMutex_);
        auto it = router_.find(req.getPath());
        if (it != router_.end())
        {
            it->second(req, resp);
            return;
        }
    }
    // 优先尝试静态文件
    if (StaticFileHandler::handle(req, resp))
    {
        DLOG_INFO << "[WebServer] 静态资源处理成功: " << req.getPath();
        return;
    }
    // 静态文件也没找到，才走 notFoundHandler
    if (notFoundHandler_)
    {
        notFoundHandler_(req, resp);
    }
    else
    {
        std::ifstream ifs("web_static/404.html");
        std::stringstream buffer;
        if (ifs)
        {
            buffer << ifs.rdbuf();
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setContentType("text/html");
            resp->setBody(buffer.str());
        }
        else
        {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setContentType("text/html");
            resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        }
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

// 自动记录日志的 SQL 执行函数
static bool execSQL(MYSQL *mysql, const std::string &sql)
{
    DLOG_INFO << "SQL: " << sql;
    if (mysql_query(mysql, sql.c_str()))
    {
        DLOG_ERROR << "SQL Error: " << mysql_error(mysql);
        return false;
    }
    DLOG_INFO << "SQL Success: " << sql;
    return true;
}

void userRegister(const HttpRequest &req, HttpResponse *resp)
{
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Method Not Allowed\"})");
        return;
    }
    std::string username, password;
    try
    {
        auto data = json::parse(req.getBody());
        username = data.at("username").get<std::string>();
        password = data.at("password").get<std::string>();
    }
    catch (json::exception &e)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Invalid JSON format or missing fields.\"})");
        DLOG_WARN << "JSON parse error: " << e.what();
        return;
    }
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Server internal error: cannot connect to DB.\"})");
        DLOG_ERROR << "Failed to get DB connection for registration.";
        return;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "INSERT INTO user(username, password) VALUES('%s', '%s')", username.c_str(), password.c_str());
    if (!execSQL(conn->m_conn, sql))
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Conflict");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Username already exists.\"})");
    }
    else
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("Created");
        resp->setContentType("application/json");
        resp->setBody(R"({\"message\":\"User registered successfully."})");
    }
}

void userLogin(const HttpRequest &req, HttpResponse *resp)
{
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Method Not Allowed\"})");
        return;
    }
    std::string username, password;
    try
    {
        auto data = json::parse(req.getBody());
        username = data.at("username").get<std::string>();
        password = data.at("password").get<std::string>();
    }
    catch (json::exception &e)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Invalid JSON format or missing fields.\"})");
        DLOG_WARN << "JSON parse error: " << e.what();
        return;
    }
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Server internal error: cannot connect to DB.\"})");
        DLOG_ERROR << "Failed to get DB connection for login.";
        return;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT password FROM user WHERE username='%s'", username.c_str());
    if (execSQL(conn->m_conn, sql))
    {
        MYSQL_RES *res = mysql_store_result(conn->m_conn);
        if (res)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && password == row[0])
            {
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("application/json");
                resp->setBody(R"({\"message\":\"Login successful."})");
            }
            else
            {
                resp->setStatusCode(HttpResponse::k400BadRequest);
                resp->setStatusMessage("Unauthorized");
                resp->setContentType("application/json");
                resp->setBody(R"({\"error\":\"Invalid username or password."})");
            }
            mysql_free_result(res);
        }
        else
        {
            resp->setStatusCode(HttpResponse::k400BadRequest);
            resp->setStatusMessage("Unauthorized");
            resp->setContentType("application/json");
            resp->setBody(R"({\"error\":\"Invalid username or password."})");
        }
    }
    else
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({\"error\":\"Server internal error: query failed."})");
    }
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