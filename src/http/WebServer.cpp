#include "http/WebServer.h"
#include "http/SocketContext.h"
#include "base/ConfigManager.h"
#include "base/MemoryPool.h"
#include "base/Buffer.h"
#include "base/ThreadPool.h"
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
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <iomanip>

/**
 * @brief 一个简单的WebSocket Echo处理器
 * @details 实现了WebSocketHandler接口，会将收到的任何消息原样返回给客户端。
 */
class EchoWebSocketHandler : public WebSocketHandler
{
public:
    /**
     * @brief 连接建立回调
     * @param conn TCP连接指针
     */
    void onConnect(const TcpConnectionPtr &conn) override
    {
        DLOG_INFO << "[WebSocket] Echo handler new connection: " << conn->peerAddress().toIpPort();
    }

    /**
     * @brief 消息接收回调
     * @param conn TCP连接指针
     * @param message 接收到的消息
     */
    void onMessage(const TcpConnectionPtr &conn, const std::string &message) override
    {
        DLOG_INFO << "[WebSocket] Echo handler received message: '" << message << "' from " << conn->peerAddress().toIpPort();
        // 将收到的消息直接发回客户端
        conn->sendWebSocket(message);
    }

    /**
     * @brief 连接关闭回调
     * @param conn TCP连接指针
     */
    void onClose(const TcpConnectionPtr &conn) override
    {
        DLOG_INFO << "[WebSocket] Echo handler connection closed: " << conn->peerAddress().toIpPort();
    }
};

using json = nlohmann::json;

static void ensureUserTableSchema();

/**
 * @brief 使用 PBKDF2-HMAC-SHA256 对密码进行加盐哈希
 * @param password 明文密码
 * @param salt 随机盐值（16 进制字符串）
 * @return 16 进制表示的哈希结果
 */
std::string hashPassword(const std::string &password, const std::string &salt)
{
    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                      (const unsigned char *)salt.c_str(), salt.length(),
                      100000, EVP_sha256(), 32, hash);

    std::stringstream ss;
    for (int i = 0; i < 32; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

/**
 * @brief 生成随机盐值
 * @return 16 进制表示的随机盐
 */
std::string generateSalt()
{
    unsigned char salt[16];
    RAND_bytes(salt, 16);

    std::stringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    return ss.str();
}

// 业务处理函数
void userLogin(const HttpRequest &req, HttpResponse *resp);
void userRegister(const HttpRequest &req, HttpResponse *resp);

// 中间件和处理器包装
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
    // 默认从"web_static"目录提供文件
    StaticFileHandler::handle(req, resp);
}

/**
 * @brief 构造函数，完成所有模块的初始化
 * @param configManager 配置管理器的引用
 *
 * 初始化流程：
 * 1. 初始化日志管理器
 * 2. 获取网络配置
 * 3. 初始化数据库连接池
 * 4. 创建事件循环
 * 5. 创建HTTP服务器
 * 6. 配置SSL（如果启用）
 * 7. 初始化回调函数
 * 8. 注册路由
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
        throw std::runtime_error("初始化失败: NetworkConfig为空。请检查配置文件是否存在或格式是否正确。");
    }

    auto dbConfig = configManager_.getDBConfig();
    if (!dbConfig || !dbConfig->isValid())
    {
        throw std::runtime_error("数据库配置无效或缺失");
    }
    DBConnectionPool::getInstance()->init(*dbConfig);
    DLOG_INFO << "[WebServer] 数据库连接池初始化完成";
    ensureUserTableSchema();

    mainLoop_ = std::make_unique<EventLoop>(networkConfig_->getEpollMode());
    InetAddress addr(networkConfig_->getPort(), networkConfig_->getIp());

    businessPool_ = std::make_unique<ThreadPool>(networkConfig_->getThreadNum());
    server_ = std::make_unique<HttpServer>(mainLoop_.get(), addr, "WebServer-01", networkConfig_, businessPool_.get());

    if (networkConfig_->isSSLEnabled())
    {
        std::string certPath = networkConfig_->getSSLCertPath();
        std::string keyPath = networkConfig_->getSSLKeyPath();
        if (certPath.empty() || keyPath.empty())
        {
            DLOG_FATAL << "SSL/TLS is enabled, but certificate or key path is not configured.";
            throw std::runtime_error("SSL/TLS配置缺失");
        }
        server_->enableSSL(certPath, keyPath);
        DLOG_INFO << "[WebServer] HTTPS服务已启用";
    }
    else
    {
        DLOG_INFO << "[WebServer] HTTP服务已启用";
    }

    initCallbacks();
    registerRoutes();
}

/**
 * @brief 析构函数
 */
WebServer::~WebServer()
{
    DLOG_INFO << "[WebServer] WebServer析构，资源将按RAII规则自动清理。";
}

/**
 * @brief 注册所有路由和中间件
 * @details 定义服务器的所有API端点和行为。
 */
void WebServer::registerRoutes()
{
    DLOG_INFO << "[WebServer] 开始注册路由...";

    // WebSocket路由
    router_.addWebSocket("/echo", std::make_shared<EchoWebSocketHandler>());

    // 全局中间件
    // 对所有请求都应用日志中间件
    router_.use(loggingMiddleware);

    // API路由
    router_.post("/api/register", userRegister);
    router_.post("/api/login", userLogin);

    // 带参数的路由示例
    router_.get("/api/users/:id/posts/:postId", [](const HttpRequest &req, HttpResponse *resp)
                {
                    json result;
                    result["message"] = "Advanced routing works!";
                    result["userId"] = req.getParam("id").value_or("not found");
                    result["postId"] = req.getParam("postId").value_or("not found");

                    resp->setStatusCode(HttpResponse::k200Ok);
                    resp->setContentType("application/json");
                    resp->setBody(result.dump(4)); });

    // 受保护的API（需要认证）
    // 请求会先通过loggingMiddleware，然后通过authMiddleware，最后到达业务处理器
    router_.get("/api/profile", authMiddleware, [](const HttpRequest &req, HttpResponse *resp)
                {
        json profile;
        profile["user_id"] = req.getUserId();
        profile["username"] = "test_user"; // 实际应从数据库查询
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(profile.dump()); });

    // 监控路由
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

    // 静态文件路由（作为所有路由的末端）
    // 使用all()方法捕获所有未被上面API路由匹配到的请求
    router_.all("/*", staticFileHandler);

    DLOG_INFO << "[WebServer] 路由注册完成。";
}

/**
 * @brief 初始化HTTP回调，将请求分发到onHttpRequest
 */
void WebServer::initCallbacks()
{
    server_->setHttpCallback(std::bind(&WebServer::onHttpRequest, this, std::placeholders::_1, std::placeholders::_2));
}

/**
 * @brief HTTP请求统一入口
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 *
 * 处理流程：
 * 1. 检查是否为WebSocket升级请求
 * 2. 如果是WebSocket请求，进行协议升级
 * 3. 如果是HTTP请求，匹配路由并执行中间件链
 * 4. 对需要认证的API统一做JWT校验和user_id注入
 */
void WebServer::onHttpRequest(const HttpRequest &req, HttpResponse *resp)
{
    auto upgradeHeader = req.getHeader("Upgrade");
    // 大小写不敏感匹配 "websocket"
    bool isWebSocketUpgrade = false;
    if (upgradeHeader)
    {
        std::string value = *upgradeHeader;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        isWebSocketUpgrade = value.find("websocket") != std::string::npos;
    }

    if (isWebSocketUpgrade)
    {
        // WebSocket协议升级处理
        WebSocketHandler::Ptr wsHandler = router_.matchWebSocket(req);
        if (wsHandler)
        {
            auto key = req.getHeader("Sec-WebSocket-Key");
            if (!key)
            {
                resp->setStatusCode(HttpResponse::k400BadRequest);
                return;
            }

            // 计算WebSocket Accept Key
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

            // 设置WebSocket升级响应头
            resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
            resp->setStatusMessage("Switching Protocols");
            resp->setHeader("Upgrade", "websocket");
            resp->setHeader("Connection", "Upgrade");
            resp->setHeader("Sec-WebSocket-Accept", acceptKey);

            // 协议升级核心逻辑
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

    // 执行中间件链
    size_t index = 0;
    const MiddlewareChain &chain = result.chain;

    // 捕获this, req, resp, chain, index，并通过值传递next本身
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

/**
 * @brief 自动记录日志的SQL执行函数
 * @param mysql MySQL连接指针
 * @param sql SQL语句
 * @return 执行是否成功
 */
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

static void setJsonError(HttpResponse *resp, HttpResponse::HttpStatusCode code, const std::string &message)
{
    resp->setStatusCode(code);
    resp->setContentType("application/json");
    resp->setBody(json({{"status", "error"}, {"message", message}}).dump());
}

static void logStmtError(const std::string &context, MYSQL_STMT *stmt)
{
    if (!stmt)
        return;
    DLOG_ERROR << context << " errno=" << mysql_stmt_errno(stmt) << " error=" << mysql_stmt_error(stmt);
}

static void ensureUserTableSchema()
{
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        DLOG_WARN << "[DB] 无法获取数据库连接，跳过user表结构检查";
        return;
    }

    execSQL(conn->m_conn,
            "CREATE TABLE IF NOT EXISTS `user` ("
            "  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
            "  `username` VARCHAR(64) NOT NULL,"
            "  `password` CHAR(64) NOT NULL,"
            "  `salt` CHAR(32) NOT NULL,"
            "  PRIMARY KEY (`id`),"
            "  UNIQUE KEY `uk_user_username` (`username`)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");

    const char *alterSql = "ALTER TABLE `user` ADD COLUMN `salt` CHAR(32) NOT NULL DEFAULT '' AFTER `password`";
    if (mysql_query(conn->m_conn, alterSql))
    {
        unsigned int err = mysql_errno(conn->m_conn);
        if (err != 1060) // ER_DUP_FIELDNAME
        {
            DLOG_ERROR << "[DB] user表结构迁移失败 errno=" << err << " error=" << mysql_error(conn->m_conn);
        }
    }
}

/**
 * @brief JWT认证检查函数
 * @param req HTTP请求对象
 * @param user_id 输出参数，用户ID
 * @return 认证是否成功
 */
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
            auto baseConfig = ConfigManager::getInstance().getBaseConfig();
            if (!baseConfig)
                return false;

            auto decoded = jwt::decode(token);

            // 验证签发者和过期时间
            auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256(baseConfig->getJwtSecret()))
                                .with_issuer(baseConfig->getJwtIssuer());

            verifier.verify(decoded);

            // 显式检查过期时间
            if (decoded.has_expires_at())
            {
                auto exp = decoded.get_expires_at();
                if (exp < std::chrono::system_clock::now())
                    return false;
            }
            else
            {
                return false; // 拒绝没有过期时间的令牌
            }

            user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
            return true;
        }
        catch (const std::exception &e)
        {
            DLOG_WARN << "[Auth] JWT验证失败: " << e.what();
            return false;
        }
        catch (...)
        {
            DLOG_WARN << "[Auth] JWT验证失败: 未知错误";
            return false;
        }
    }
    return false;
}

/**
 * @brief 用户注册处理函数
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
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

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误，无法连接数据库");
        return;
    }

    // 生成盐值并使用PBKDF2哈希密码
    std::string salt = generateSalt();
    std::string hashedPassword = hashPassword(password, salt);

    // 使用预编译语句防止SQL注入
    MYSQL_STMT *stmt = mysql_stmt_init(conn->m_conn);
    if (!stmt)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    const char *sql = "INSERT INTO `user`(`username`, `password`, `salt`) VALUES(?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
    {
        logStmtError("[Register] mysql_stmt_prepare失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库表结构或SQL异常）");
        return;
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)username.c_str();
    bind[0].buffer_length = username.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)hashedPassword.c_str();
    bind[1].buffer_length = hashedPassword.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char *)salt.c_str();
    bind[2].buffer_length = salt.length();

    if (mysql_stmt_bind_param(stmt, bind))
    {
        logStmtError("[Register] mysql_stmt_bind_param失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_execute(stmt))
    {
        unsigned int err = mysql_stmt_errno(stmt);
        logStmtError("[Register] mysql_stmt_execute失败", stmt);
        mysql_stmt_close(stmt);
        if (err == 1062) // ER_DUP_ENTRY
        {
            setJsonError(resp, HttpResponse::k409Conflict, "用户名已存在");
        }
        else
        {
            setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库执行失败）");
        }
        return;
    }

    mysql_stmt_close(stmt);
    resp->setStatusCode(HttpResponse::k201Created);
    resp->setContentType("application/json");
    resp->setBody(R"({"status":"success", "message":"用户注册成功"})");
}

/**
 * @brief 用户登录处理函数
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 */
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

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误，无法连接数据库");
        return;
    }

    // 使用预编译语句防止SQL注入
    MYSQL_STMT *stmt = mysql_stmt_init(conn->m_conn);
    if (!stmt)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    const char *sql = "SELECT `id`, `password`, `salt` FROM `user` WHERE `username`=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
    {
        logStmtError("[Login] mysql_stmt_prepare失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库表结构或SQL异常）");
        return;
    }

    // 绑定输入参数（用户名）
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = (char *)username.c_str();
    bind_param[0].buffer_length = username.length();

    if (mysql_stmt_bind_param(stmt, bind_param))
    {
        logStmtError("[Login] mysql_stmt_bind_param失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_execute(stmt))
    {
        logStmtError("[Login] mysql_stmt_execute失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库执行失败）");
        return;
    }

    // 准备接收查询结果
    int user_id;
    char db_password[128];
    char db_salt[64];
    unsigned long password_len, salt_len;

    // 绑定输出结果
    MYSQL_BIND bind_result[3];
    memset(bind_result, 0, sizeof(bind_result));

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = &user_id;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = db_password;
    bind_result[1].buffer_length = sizeof(db_password);
    bind_result[1].length = &password_len;

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = db_salt;
    bind_result[2].buffer_length = sizeof(db_salt);
    bind_result[2].length = &salt_len;

    if (mysql_stmt_bind_result(stmt, bind_result))
    {
        logStmtError("[Login] mysql_stmt_bind_result失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_store_result(stmt))
    {
        logStmtError("[Login] mysql_stmt_store_result失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_fetch(stmt) == 0)
    {
        // 获取数据库中的密码哈希和盐值
        std::string dbPasswordHash(db_password, password_len);
        std::string salt(db_salt, salt_len);
        std::string hashedPassword = hashPassword(password, salt);

        if (hashedPassword == dbPasswordHash)
        {
            // 密码验证成功，生成JWT令牌
            auto baseConfig = ConfigManager::getInstance().getBaseConfig();
            if (!baseConfig)
            {
                mysql_stmt_close(stmt);
                setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（JWT配置缺失）");
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

    mysql_stmt_close(stmt);
}

/**
 * @brief 启动服务器
 * @details 启动包括日志、线程池、网络服务、主事件循环
 */
void WebServer::start()
{
    if (running_.exchange(true))
        return;
    DLOG_INFO << "[WebServer] 启动...";
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
    if (!mainLoop_->isInLoopThread())
    {
        mainLoop_->quit();
    }
}
