#include "http/WebServer.h"
#include "http/SocketContext.h"
#include "http/StaticFileHandler.h"
#include "http/handlers/AuthHandler.h"
#include "http/handlers/UserHandler.h"
#include "http/handlers/Middleware.h"
#include "http/handlers/DemoHandlers.h"
#include "http/handlers/PerfHandlers.h"
#include "http/handlers/StatsHandler.h"
#include "base/ConfigManager.h"
#include "base/PerfConfig.h"
#include "base/ThreadPool.h"
#include "net/NetworkConfig.h"
#include "log/LogManager.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief 静态文件处理器
 */
static void staticFileHandler(const HttpRequest &req, HttpResponse *resp)
{
    StaticFileHandler::handle(req, resp);
}

/**
 * @brief 构造函数，完成所有模块的初始化
 * @param configManager 配置管理器的引用
 *
 * 初始化流程：
 * 1. 初始化日志管理器
 * 2. 获取网络配置
 * 3. 创建事件循环
 * 4. 创建HTTP服务器
 * 5. 配置SSL（如果启用）
 * 6. 初始化回调函数
 * 7. 注册路由
 *
 * @note 数据库初始化已移至 Bootstrap::initDatabase()
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

    initPerfHandlers(configManager_.getPerfConfig());
    initCallbacks();
    registerRoutes();

    // 注册配置更新回调
    configManager_.registerUpdateCallback("WebServer", [this]()
                                          {
        DLOG_INFO << "[WebServer] 配置已更新，准备应用新配置";
        onConfigUpdate(); });
}

/**
 * @brief 析构函数
 */
WebServer::~WebServer()
{
    // 注销配置更新回调
    configManager_.unregisterUpdateCallback("WebServer");
    DLOG_INFO << "[WebServer] WebServer析构，资源将按RAII规则自动清理。";
}

/**
 * @brief 注册所有路由和中间件
 * @details 定义服务器的所有API端点和行为。
 */
void WebServer::registerRoutes()
{
    DLOG_INFO << "[WebServer] 开始注册路由...";

    // [DEMO] WebSocket路由
    router_.addWebSocket("/echo", std::make_shared<EchoWebSocketHandler>());
    router_.addWebSocket("/ws/chat", std::make_shared<ChatWebSocketHandler>());

    // 全局中间件
    router_.use(loggingMiddleware);

    // API路由
    router_.post("/api/register", userRegister);
    router_.post("/api/login", userLogin);

    // [DEMO] 带参数的路由示例
    router_.get("/api/users/:id/posts/:postId", demoRouteParamsHandler);

    // [DEMO] 受保护的API
    router_.get("/api/profile", authMiddleware, demoProtectedHandler);

    // 监控路由
    router_.get("/debug/stats", statsHandler);
    router_.get("/debug/perf-stats", perfStatsHandler);

    // perf 路由
    router_.get("/perf/ping", perfPingHandler);
    router_.get("/perf/json", perfJsonHandler);
    router_.post("/perf/echo-json", perfEchoJsonHandler);
    router_.get("/perf/items", perfItemsHandler);
    router_.post("/perf/items/batch", perfBatchItemsHandler);
    router_.get("/perf/compute", perfComputeHandler);
    router_.all("/perf/file/:name", perfFileHandler);

    // 静态文件路由
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

/**
 * @brief 配置更新回调，应用新配置
 * @note 配置更新仅对新建连接生效，已有连接保持旧配置直到下次重置定时器
 */
void WebServer::onConfigUpdate()
{
    // 在EventLoop线程中执行配置更新
    mainLoop_->runInLoop([this]()
                         {
        auto newNetworkConfig = configManager_.getNetworkConfig();
        auto newPerfConfig = configManager_.getPerfConfig();
        if (!newNetworkConfig)
        {
            DLOG_WARN << "[WebServer] 配置更新失败：NetworkConfig为空";
            return;
        }

        // 更新网络配置
        networkConfig_ = newNetworkConfig;

        // 更新 TcpServer 的配置（仅影响新连接）
        if (server_)
        {
            server_->updateNetworkConfig(newNetworkConfig);
            DLOG_INFO << "[WebServer] 已应用新的网络配置（仅新连接生效），空闲超时: " << newNetworkConfig->getIdleTimeout() << "秒";
        }

        refreshPerfHandlers(newPerfConfig);

        DLOG_INFO << "[WebServer] 配置热重载完成"; });
}
