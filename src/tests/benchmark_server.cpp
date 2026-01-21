#include <benchmark/benchmark.h>
#include "http/WebServer.h"
#include "base/ConfigManager.h"
#include "log/LogManager.h"
#include <chrono>
#include <future>
#include <csignal>
#include <curl/curl.h>
#include <string>
#include <iostream>

// --- 全局服务器实例 ---
// 我们在所有基准测试之外启动一个服务器实例，以模拟真实运行环境。
std::unique_ptr<WebServer> g_server;
std::unique_ptr<std::thread> g_server_thread;
std::shared_ptr<NetworkConfig> g_net_config;
std::string g_base_url;
bool g_use_ssl = false;

// --- CURL 相关函数 ---
// 回调函数：丢弃响应体数据（节省内存，不处理内容）
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    return size * nmemb;
}

// HTTPS请求函数，支持GET/POST，跳过证书验证（自签证书适用）
// 返回HTTP状态码，失败返回-1
int simple_http_request(const std::string &url,
                        const std::string &method = "GET",
                        const std::string &body = "")
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    // 禁用代理，避免本地请求走系统代理导致阻塞
    curl_easy_setopt(curl, CURLOPT_PROXY, "");
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "127.0.0.1,localhost");
    // 避免卡死：设置连接/总超时
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);

    if (method == "POST")
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());

        // 设置 Content-Type 为 application/json
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (g_use_ssl)
    {
        // 跳过证书验证（自签证书环境下必需）
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // 忽略响应体内容
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    CURLcode res = curl_easy_perform(curl);
    long response_code = 0;
    if (res == CURLE_OK)
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    }
    else
    {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);
    return static_cast<int>(response_code);
}

static void wait_for_server_ready()
{
    // 轮询等待服务就绪，避免固定 sleep 导致偶发连接失败
    const int max_attempts = 30;
    const int wait_ms = 200;
    std::string url = g_base_url + "/";
    for (int i = 0; i < max_attempts; ++i)
    {
        int status = simple_http_request(url, "GET");
        if (status == 200 || status == 404)
        {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }
}

// --- 内部基准测试用例 ---
// 这个函数定义了我们要测试的具体操作。
// 在这个例子中，我们模拟一个最简单的 HTTP GET 请求。
static void BM_SimpleGetRequest(benchmark::State &state)
{
    // 构造一个极简的 HTTP GET 请求
    HttpRequest req;
    std::string path = "/";
    req.setMethod("GET");
    req.setPath(path.c_str(), path.c_str() + path.length());

    // 这是 benchmark 库的循环。它会自动运行很多次来获取稳定的性能数据。
    for (auto _ : state)
    {
        HttpResponse resp(true); // 每次循环都创建一个新的 response 对象

        // 直接调用 router 的 match 和处理链，绕过网络层，
        // 从而精确地测试路由和业务逻辑的处理性能。
        auto result = g_server->getRouter().match("GET", "/");
        if (result.matched && !result.chain.empty())
        {
            // 模拟中间件调用
            auto next = [] {};
            result.chain.front()(req, &resp, next);
        }

        // 使用 benchmark::DoNotOptimize 防止编译器优化掉 resp 对象，
        // 确保我们的测试代码被真实执行。
        benchmark::DoNotOptimize(resp);
    }
}

// --- 真实网络请求基准测试 ---
// Benchmark 测试 - GET https://127.0.0.1:8443/
static void BM_RealHttpGetRoot(benchmark::State &state)
{
    std::string url = g_base_url + "/";
    for (auto _ : state)
    {
        int status_code = simple_http_request(url, "GET");
        benchmark::DoNotOptimize(status_code);
        if (status_code != 200)
        {
            state.SkipWithError(("请求失败，状态码：" + std::to_string(status_code)).c_str());
        }
    }
}

// Benchmark 测试 - POST https://127.0.0.1:8443/api/login
static void BM_RealHttpPostLogin(benchmark::State &state)
{
    std::string url = g_base_url + "/api/login";
    std::string body = R"({"username":"ayp","password":"123456"})";
    for (auto _ : state)
    {
        int status_code = simple_http_request(url, "POST", body);
        benchmark::DoNotOptimize(status_code);
        if (status_code != 200)
        {
            state.SkipWithError(("请求失败，状态码：" + std::to_string(status_code)).c_str());
        }
    }
}

// --- 注册基准测试 ---
// 将我们的测试用例注册到 benchmark 框架中。
BENCHMARK(BM_SimpleGetRequest);
BENCHMARK(BM_RealHttpGetRoot)->Threads(8);
BENCHMARK(BM_RealHttpPostLogin)->Threads(8);

// --- 基准测试主函数 ---
int main(int argc, char **argv)
{
    // 1. 初始化日志系统，并设置到最高级别（FATAL），
    //    以最大限度地减少日志 I/O 对性能测试结果的干扰。
    initDefaultLogger();
    LogManager::getInstance()->getRoot()->setLevel(Level::FATAL);

    // 2. 加载配置
    ConfigManager::getInstance().load("configs/config.yml");
    g_net_config = ConfigManager::getInstance().getNetworkConfig();
    if (!g_net_config)
    {
        DLOG_FATAL << "网络配置加载失败，基准测试无法运行。";
        return 1;
    }

    g_use_ssl = g_net_config->isSSLEnabled();
    std::string host = g_net_config->getIp();
    if (host.empty() || host == "0.0.0.0")
    {
        host = "127.0.0.1";
    }
    std::string scheme = g_use_ssl ? "https" : "http";
    g_base_url = scheme + "://" + host + ":" + std::to_string(g_net_config->getPort());

    // 3. 在一个独立的线程中启动服务器
    //    这样做可以确保服务器的事件循环不会阻塞基准测试的运行。
    g_server = std::make_unique<WebServer>(ConfigManager::getInstance());
    g_server_thread = std::make_unique<std::thread>([&]()
                                                    { g_server->start(); });

    // 4. 初始化curl全局环境
    curl_global_init(CURL_GLOBAL_ALL);

    // 等待服务器完全启动
    wait_for_server_ready();
    DLOG_FATAL << "服务器已启动，开始运行基准测试...";

    // 5. 初始化并运行所有已注册的基准测试
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    // 6. 清理curl全局环境
    curl_global_cleanup();

    // 7. 停止服务器并清理资源（添加超时机制）
    g_server->stop();

    // 使用超时机制等待服务器线程结束
    auto future = std::async(std::launch::async, [&]()
                             {
        if (g_server_thread->joinable()) {
            g_server_thread->join();
        } });

    // 等待最多3秒
    if (future.wait_for(std::chrono::seconds(3)) == std::future_status::timeout)
    {
        // 强制退出，不等待线程
    }

    DLOG_FATAL << "基准测试完成。";

    std::raise(SIGINT);
    return 0;
}
