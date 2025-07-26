#include <benchmark/benchmark.h>
#include "http/WebServer.h"
#include "base/ConfigManager.h"
#include "log/LogManager.h"

// --- 全局服务器实例 ---
// 我们在所有基准测试之外启动一个服务器实例，以模拟真实运行环境。
std::unique_ptr<WebServer> g_server;
std::unique_ptr<std::thread> g_server_thread;
std::shared_ptr<NetworkConfig> g_net_config;

// --- 基准测试用例 ---
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

// --- 注册基准测试 ---
// 将我们的测试用例注册到 benchmark 框架中。
BENCHMARK(BM_SimpleGetRequest);

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

    // 3. 在一个独立的线程中启动服务器
    //    这样做可以确保服务器的事件循环不会阻塞基准测试的运行。
    g_server = std::make_unique<WebServer>(ConfigManager::getInstance());
    g_server_thread = std::make_unique<std::thread>([&]()
                                                    { g_server->start(); });

    // 等待服务器完全启动 (这是一个简化的等待，实际项目可能需要更可靠的机制)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    DLOG_FATAL << "服务器已启动，开始运行基准测试...";

    // 4. 初始化并运行所有已注册的基准测试
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    // 5. 停止服务器并清理资源
    g_server->stop();
    if (g_server_thread->joinable())
    {
        g_server_thread->join();
    }

    DLOG_FATAL << "基准测试完成。";

    return 0;
}