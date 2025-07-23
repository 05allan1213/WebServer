#include "http/WebServer.h"
#include "base/ConfigManager.h"
#include "base/MemoryPool.h"
#include "base/Buffer.h"
#include "net/NetworkConfig.h"
#include "db/DBConfig.h"
#include "base/BaseConfig.h"
#include "db/DBConnectionPool.h"
#include "http/StaticFileHandler.h"
#include "log/LogManager.h"
#include <csignal>
#include <memory>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

using json = nlohmann::json;

// --- 业务处理函数 ---
void userLogin(const HttpRequest &req, HttpResponse *resp);
void userRegister(const HttpRequest &req, HttpResponse *resp);

// --- 中间件和处理器包装 ---
/**
 * @brief 日志中间件
 * @details 记录每个请求的开始和结束，以及处理耗时。
 */
void loggingMiddleware(const HttpRequest &req, HttpResponse *resp, Next next)
{
    auto start = std::chrono::high_resolution_clock::now();
    DLOG_INFO << "--> " << req.getMethodString() << " " << req.getPath();

    next(); // 调用后续中间件或处理器

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    DLOG_INFO << "<-- " << req.getMethodString() << " " << req.getPath()
              << " " << resp->getStatusCode() << " " << duration.count() << "us";
}

/**
 * @brief 认证中间件
 * @details 检查JWT，如果通过则调用next()，否则直接返回403。
 */
void authMiddleware(const HttpRequest &req, HttpResponse *resp, Next next)
{
    int user_id = -1;
    if (checkAuth(req, user_id))
    {
        const_cast<HttpRequest &>(req).setUserId(user_id);
        DLOG_INFO << "[Auth] 认证成功, user_id: " << user_id;
        next(); // 认证成功，继续处理
    }
    else
    {
        DLOG_WARN << "[Auth] 认证失败, 路径: " << req.getPath();
        resp->setStatusCode(HttpResponse::k403Forbidden);
        resp->setBody("{\"error\":\"Forbidden\"}");
        resp->setContentType("application/json");
        // 认证失败，中断请求链
    }
}

/**
 * @brief 静态文件处理器
 * @details 这是一个HttpHandler，作为中间件链的终点。
 */
void staticFileHandler(const HttpRequest &req, HttpResponse *resp)
{
    // 默认从 "web_static" 目录提供文件
    StaticFileHandler::handle(req, resp);
}

class ThreadPool
{
public:
    void start() {}
    void stop() {}
};

static std::unique_ptr<WebServer> g_server;

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
    try
    {
        // 1. 启动最小化的引导日志
        initDefaultLogger();

        // 2. 初始化配置管理器
        auto &configMgr = ConfigManager::getInstance();
        configMgr.load("configs/config.yml", 5);

        // 3. 关键检查：确认核心配置是否加载成功
        if (!configMgr.getNetworkConfig() || !configMgr.getLogConfig() ||
            !configMgr.getDBConfig() || !configMgr.getBaseConfig())
        {

            DLOG_FATAL << "[WebServer] 核心配置加载失败，服务器无法启动。请检查配置文件路径和内容。";
            std::cerr << "[WebServer] 核心配置加载失败，服务器无法启动。请检查配置文件路径和内容。" << std::endl;
            exit(1);
        }

        // 4. 初始化内存池 (在加载配置之后，创建任何Buffer之前)
        MemoryPool::getInstance();

        // 5. 根据加载的配置，重新初始化功能完备的日志系统
        initLogSystem();

        // 6. 创建并运行WebServer
        g_server = std::make_unique<WebServer>(configMgr);
        std::signal(SIGINT, shutdown_handler);
        std::signal(SIGTERM, shutdown_handler);

        DLOG_INFO << "[WebServer] 准备启动服务...";
        g_server->start();
    }
    catch (const std::exception &e)
    {
        DLOG_FATAL << "[WebServer] 启动时发生未捕获异常: " << e.what();
        std::cerr << "[WebServer] 启动时发生未捕获异常: " << e.what() << std::endl;
        exit(1);
    }
}

/**
 * @brief 构造函数，完成所有模块的初始化
 * @param configManager 配置管理器的引用
 */
WebServer::WebServer(ConfigManager &configManager)
    : running_(false),
      configManager_(configManager),
      router_()
{
    logManager_ = LogManager::getInstance();

    networkConfig_ = configManager_.getNetworkConfig();
    if (!networkConfig_)
    {
        throw std::runtime_error("初始化失败: NetworkConfig 为空。请检查配置文件是否存在或格式是否正确。");
    }

    auto dbConfig = configManager_.getDBConfig();
    if (!dbConfig || !dbConfig->isValid())
    {
        throw std::runtime_error("数据库配置无效或缺失");
    }
    DBConnectionPool::getInstance()->init(*dbConfig);
    DLOG_INFO << "[WebServer] 数据库连接池初始化完成";

    mainLoop_ = std::make_unique<EventLoop>(networkConfig_->getEpollMode());
    InetAddress addr(networkConfig_->getPort(), networkConfig_->getIp());

    server_ = std::make_unique<HttpServer>(mainLoop_.get(), addr, "WebServer-01", networkConfig_);

    businessPool_ = std::make_unique<ThreadPool>();
    initCallbacks();
    registerRoutes();
}

WebServer::~WebServer()
{
    stop();
}

/**
 * @brief 注册所有路由和中间件
 * @details 定义服务器的所有 API 端点和行为。
 */
void WebServer::registerRoutes()
{
    DLOG_INFO << "[WebServer] 开始注册路由...";

    // --- 全局中间件 ---
    // 对所有请求都应用日志中间件。
    router_.use(loggingMiddleware);

    // --- API 路由 ---
    router_.post("/api/register", userRegister);
    router_.post("/api/login", userLogin);

    // --- 受保护的 API (需要认证) ---
    // 请求会先通过 loggingMiddleware，然后通过 authMiddleware，最后到达业务处理器。
    router_.get("/api/profile", authMiddleware, [](const HttpRequest &req, HttpResponse *resp)
                {
        json profile;
        profile["user_id"] = req.getUserId();
        profile["username"] = "test_user"; // 实际应从数据库查询
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(profile.dump()); });

    // --- 监控路由 ---
    router_.get("/debug/stats", [this](const HttpRequest &req, HttpResponse *resp)
                {
        json stats;
        stats["buffer"]["active_count"] = Buffer::getActiveBuffers();
        stats["buffer"]["pool_memory_bytes"] = Buffer::getPoolMemory();
        stats["buffer"]["heap_memory_bytes"] = Buffer::getHeapMemory();
        stats["buffer"]["resize_count"] = Buffer::getResizeCount();
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(stats.dump(4)); });

    // --- 静态文件路由 (作为所有路由的末端) ---
    // 使用 all() 方法捕获所有未被上面 API 路由匹配到的请求。
    router_.all("/*", staticFileHandler);

    DLOG_INFO << "[WebServer] 路由注册完成。";
}

/**
 * @brief 初始化 HTTP 回调，将请求分发到 onHttpRequest
 */
void WebServer::initCallbacks()
{
    server_->setHttpCallback(std::bind(&WebServer::onHttpRequest, this, std::placeholders::_1, std::placeholders::_2));
}

/**
 * @brief HTTP请求统一入口，根据 path 路由到静态或动态处理，并对需要认证的API统一做JWT校验和user_id注入
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
void WebServer::onHttpRequest(const HttpRequest &req, HttpResponse *resp)
{
    const char *methodStr = req.getMethodString();
    const std::string &path = req.getPath();
    DLOG_INFO << "[WebServer] 收到HTTP请求: " << methodStr << " " << path;
    RouteMatchResult result = router_.match(methodStr, path);

    if (!result.matched || result.chain.empty())
    {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        resp->setContentType("text/html");
        DLOG_WARN << "[WebServer] 404 Not Found: " << path;
        return;
    }

    // --- 执行中间件链 ---
    size_t index = 0;
    const MiddlewareChain &chain = result.chain;

    // 捕获 this, req, resp, chain, index, 并通过值传递 next 本身
    std::function<void()> next;
    next = [&]()
    {
        if (index < chain.size())
        {
            const auto &middleware = chain[index++];
            middleware(req, resp, next);
        }
    };

    // 启动链
    next();
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

/// JWT认证检查函数
bool checkAuth(const HttpRequest &req, int &user_id)
{
    auto authOpt = req.getHeader("Authorization");
    if (!authOpt.has_value())
        return false;

    const std::string &auth = authOpt.value();
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ")
    {
        std::string token = auth.substr(7);
        try
        {
            // 从 ConfigManager 获取最新的 JWT Secret
            auto baseConfig = ConfigManager::getInstance().getBaseConfig();
            if (!baseConfig)
                return false;

            auto decoded = jwt::decode(token);
            // 使用 () 构造函数，而不是 {}
            auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256(baseConfig->getJwtSecret()));
            verifier.verify(decoded);
            user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    return false;
}

void userRegister(const HttpRequest &req, HttpResponse *resp)
{
    DLOG_INFO << "[WebServer] 用户注册请求: " << req.getBody();
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        DLOG_WARN << "[WebServer] 非POST方法注册请求";
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Method Not Allowed"})");
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
        DLOG_WARN << "[WebServer] 注册JSON解析失败: " << e.what();
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Invalid JSON format or missing fields."})");
        return;
    }
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        DLOG_ERROR << "[WebServer] 注册时数据库连接获取失败";
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Server internal error: cannot connect to DB."})");
        return;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "INSERT INTO user(username, password) VALUES('%s', '%s')", username.c_str(), password.c_str());
    if (!execSQL(conn->m_conn, sql))
    {
        DLOG_WARN << "[WebServer] 注册失败, 用户名已存在: " << username;
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Conflict");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Username already exists."})");
    }
    else
    {
        DLOG_INFO << "[WebServer] 用户注册成功: " << username;
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("Created");
        resp->setContentType("application/json");
        resp->setBody(R"({"message":"User registered successfully."})");
    }
}

void userLogin(const HttpRequest &req, HttpResponse *resp)
{
    DLOG_INFO << "[WebServer] 用户登录请求: " << req.getBody();
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        DLOG_WARN << "[WebServer] 非POST方法登录请求";
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Method Not Allowed"})");
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
        DLOG_WARN << "[WebServer] 登录JSON解析失败: " << e.what();
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Invalid JSON format or missing fields."})");
        return;
    }
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        DLOG_ERROR << "[WebServer] 登录时数据库连接获取失败";
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Server internal error: cannot connect to DB."})");
        return;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT id, password FROM user WHERE username='%s'", username.c_str());
    if (execSQL(conn->m_conn, sql))
    {
        MYSQL_RES *res = mysql_store_result(conn->m_conn);
        if (res)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (!row)
            {
                DLOG_WARN << "[WebServer] 登录失败, 用户未注册: " << username;
                resp->setStatusCode(HttpResponse::k400BadRequest);
                resp->setStatusMessage("Unauthorized");
                resp->setContentType("application/json");
                resp->setBody(R"({"error":"未注册请先注册"})");
            }
            else if (password == row[1])
            {
                int user_id = atoi(row[0]);
                auto baseConfig = ConfigManager::getInstance().getBaseConfig();
                if (!baseConfig)
                {
                    DLOG_ERROR << "[WebServer] 登录时获取JWT配置失败";
                    resp->setStatusCode(HttpResponse::k500InternalServerError);
                    resp->setStatusMessage("Internal Error");
                    resp->setContentType("application/json");
                    resp->setBody(R"({"error":"Server internal error: config error."})");
                    return;
                }
                std::string secret = baseConfig->getJwtSecret();
                int expire = baseConfig->getJwtExpireSeconds();
                std::string issuer = baseConfig->getJwtIssuer();
                auto token = jwt::create()
                                 .set_issuer(issuer)
                                 .set_type("JWS")
                                 .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
                                 .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds(expire))
                                 .sign(jwt::algorithm::hs256(secret));
                json resp_json = {{"token", token}};
                DLOG_INFO << "[WebServer] 用户登录成功: " << username << ", user_id=" << user_id;
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("application/json");
                resp->setBody(resp_json.dump());
            }
            else
            {
                DLOG_WARN << "[WebServer] 登录失败, 密码错误: " << username;
                resp->setStatusCode(HttpResponse::k400BadRequest);
                resp->setStatusMessage("Unauthorized");
                resp->setContentType("application/json");
                resp->setBody(R"({"error":"用户名或密码错误"})");
            }
            mysql_free_result(res);
        }
        else
        {
            DLOG_WARN << "[WebServer] 登录失败, 用户未注册: " << username;
            resp->setStatusCode(HttpResponse::k400BadRequest);
            resp->setStatusMessage("Unauthorized");
            resp->setContentType("application/json");
            resp->setBody(R"({"error":"未注册请先注册"})");
        }
    }
    else
    {
        DLOG_ERROR << "[WebServer] 登录时数据库查询失败: " << username;
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Error");
        resp->setContentType("application/json");
        resp->setBody(R"({"error":"Server internal error: query failed."})");
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