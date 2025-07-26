#include "http/WebServer.h"
#include "http/SocketContext.h"
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
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <iomanip>

/**
 * @brief 一个简单的 WebSocket Echo 处理器
 * @details 实现了 WebSocketHandler 接口，会将收到的任何消息原样返回给客户端。
 */
class EchoWebSocketHandler : public WebSocketHandler
{
public:
    void onConnect(const TcpConnectionPtr &conn) override
    {
        DLOG_INFO << "[WebSocket] Echo handler new connection: " << conn->peerAddress().toIpPort();
    }

    void onMessage(const TcpConnectionPtr &conn, const std::string &message) override
    {
        DLOG_INFO << "[WebSocket] Echo handler received message: '" << message << "' from " << conn->peerAddress().toIpPort();
        // 将收到的消息直接发回客户端
        conn->sendWebSocket(message);
    }

    void onClose(const TcpConnectionPtr &conn) override
    {
        DLOG_INFO << "[WebSocket] Echo handler connection closed: " << conn->peerAddress().toIpPort();
    }
};

using json = nlohmann::json;

// --- 密码哈希辅助函数 ---
/**
 * @brief 计算字符串的SHA-256哈希值
 * @param str 输入字符串
 * @return 16进制表示的哈希值
 */
std::string sha256(const std::string &str)
{
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    EVP_DigestInit_ex(context, md, NULL);
    EVP_DigestUpdate(context, str.c_str(), str.size());
    EVP_DigestFinal_ex(context, hash, &lengthOfHash);
    EVP_MD_CTX_free(context);

    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

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

    if (networkConfig_->isSSLEnabled())
    {
        std::string certPath = networkConfig_->getSSLCertPath();
        std::string keyPath = networkConfig_->getSSLKeyPath();
        if (certPath.empty() || keyPath.empty())
        {
            DLOG_FATAL << "SSL/TLS is enabled, but certificate or key path is not configured.";
            throw std::runtime_error("SSL/TLS 配置缺失");
        }
        server_->enableSSL(certPath, keyPath);
        DLOG_INFO << "[WebServer] HTTPS 服务已启用";
    }
    else
    {
        DLOG_INFO << "[WebServer] HTTP 服务已启用";
    }

    businessPool_ = std::make_unique<ThreadPool>();
    initCallbacks();
    registerRoutes();
}

WebServer::~WebServer()
{
    DLOG_INFO << "[WebServer] WebServer 析构，资源将按RAII规则自动清理。";
}

/**
 * @brief 注册所有路由和中间件
 * @details 定义服务器的所有 API 端点和行为。
 */
void WebServer::registerRoutes()
{
    DLOG_INFO << "[WebServer] 开始注册路由...";

    // --- WebSocket 路由 ---
    router_.addWebSocket("/echo", std::make_shared<EchoWebSocketHandler>());

    // --- 全局中间件 ---
    // 对所有请求都应用日志中间件。
    router_.use(loggingMiddleware);

    // --- API 路由 ---
    router_.post("/api/register", userRegister);
    router_.post("/api/login", userLogin);

    // --- 带参数的路由示例 ---
    router_.get("/api/users/:id/posts/:postId", [](const HttpRequest &req, HttpResponse *resp)
                {
                    json result;
                    result["message"] = "Advanced routing works!";
                    result["userId"] = req.getParam("id").value_or("not found");
                    result["postId"] = req.getParam("postId").value_or("not found");

                    resp->setStatusCode(HttpResponse::k200Ok);
                    resp->setContentType("application/json");
                    resp->setBody(result.dump(4)); });

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
    auto upgradeHeader = req.getHeader("Upgrade");
    if (upgradeHeader && upgradeHeader->find("websocket") != std::string::npos)
    {
        WebSocketHandler::Ptr wsHandler = router_.matchWebSocket(req);
        if (wsHandler)
        {
            auto key = req.getHeader("Sec-WebSocket-Key");
            if (!key)
            {
                resp->setStatusCode(HttpResponse::k400BadRequest);
                return;
            }
            std::string combined = key.value() + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            unsigned char hash[SHA_DIGEST_LENGTH];
            SHA1(reinterpret_cast<const unsigned char *>(combined.c_str()), combined.length(), hash);

            BIO *b64 = BIO_new(BIO_f_base64());
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            BIO *mem = BIO_new(BIO_s_mem());
            BIO_push(b64, mem);
            BIO_write(b64, hash, SHA_DIGEST_LENGTH);
            BIO_flush(b64);
            BUF_MEM *bptr;
            BIO_get_mem_ptr(b64, &bptr);
            std::string acceptKey(bptr->data, bptr->length);
            BIO_free_all(b64);

            resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
            resp->setStatusMessage("Switching Protocols");
            resp->setHeader("Upgrade", "websocket");
            resp->setHeader("Connection", "Upgrade");
            resp->setHeader("Sec-WebSocket-Accept", acceptKey);

            // --- 协议升级核心逻辑 ---
            auto conn = std::any_cast<TcpConnectionPtr>(req.getContext());
            auto context = std::any_cast<std::shared_ptr<SocketContext>>(*conn->getMutableContext());
            context->state = SocketContext::WEBSOCKET; // 切换状态
            context->wsHandler = wsHandler;            // 绑定处理器

            // 触发连接建立回调
            wsHandler->onConnect(conn);

            return;
        }
    }

    // 如果不是WebSocket请求，则执行HTTP中间件链
    const char *methodStr = req.getMethodString();
    const std::string &path = req.getPath();
    DLOG_INFO << "[WebServer] 收到HTTP请求: " << methodStr << " " << path;

    // 匹配路由
    RouteMatchResult result = router_.match(methodStr, path);
    // 如果未匹配到，则返回404
    if (!result.matched || result.chain.empty())
    {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        resp->setContentType("text/html");
        DLOG_WARN << "[WebServer] 404 Not Found: " << path;
        return;
    }
    // 将匹配到的路径参数设置到请求对象中
    const_cast<HttpRequest &>(req).setParams(result.params);

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
        resp->setStatusCode(HttpResponse::k400BadRequest);
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
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"请求格式错误或缺少字段"})");
        return;
    }

    std::string hashedPassword = sha256(password);

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"服务器内部错误，无法连接数据库"})");
        return;
    }

    char sql[512];
    snprintf(sql, sizeof(sql), "INSERT INTO user(username, password) VALUES('%s', '%s')",
             username.c_str(), hashedPassword.c_str());

    if (!execSQL(conn->m_conn, sql))
    {
        resp->setStatusCode(HttpResponse::k409Conflict);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"用户名已存在"})");
    }
    else
    {
        resp->setStatusCode(HttpResponse::k201Created);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"success", "message":"用户注册成功"})");
    }
}

void userLogin(const HttpRequest &req, HttpResponse *resp)
{
    DLOG_INFO << "[WebServer] 用户登录请求: " << req.getBody();
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
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
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"请求格式错误或缺少字段"})");
        return;
    }

    std::string hashedPassword = sha256(password);

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        return;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, password FROM user WHERE username='%s'", username.c_str());

    if (mysql_query(conn->m_conn, sql) == 0)
    {
        MYSQL_RES *res = mysql_store_result(conn->m_conn);
        if (res && mysql_num_rows(res) > 0)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            std::string dbPasswordHash = row[1];

            if (hashedPassword == dbPasswordHash)
            {
                int user_id = atoi(row[0]);
                auto baseConfig = ConfigManager::getInstance().getBaseConfig();
                if (!baseConfig)
                {
                    resp->setStatusCode(HttpResponse::k500InternalServerError);
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

                json resp_json = {{"status", "success"}, {"token", token}};
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setContentType("application/json");
                resp->setBody(resp_json.dump());
            }
            else
            {
                resp->setStatusCode(HttpResponse::k401Unauthorized);
                resp->setContentType("application/json");
                resp->setBody(R"({"status":"error", "message":"用户名或密码错误"})");
            }
        }
        else
        {
            resp->setStatusCode(HttpResponse::k401Unauthorized);
            resp->setContentType("application/json");
            resp->setBody(R"({"status":"error", "message":"用户名或密码错误"})");
        }
        mysql_free_result(res);
    }
    else
    {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"服务器内部错误"})");
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
    if (!mainLoop_->isInLoopThread())
    {
        mainLoop_->quit();
    }
}