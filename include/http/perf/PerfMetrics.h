#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

/**
 * @brief 性能平台运行时指标聚合器
 *
 * 负责聚合 HTTP 请求、连接状态和 WebSocket 广播等实时指标，
 * 为 `/debug/perf-stats` 和前端控制台提供统一数据快照。
 */
class PerfMetrics
{
public:
    static PerfMetrics &instance();

    void setDbModeEnabled(bool enabled);
    void setPerfDbAvailable(bool available);
    void setMemoryDatasetSize(size_t size);

    void onTcpConnectionOpened();
    void onTcpConnectionClosed();
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessage();
    void onWebSocketBroadcast(size_t fanout);

    void recordHttp(const std::string &path,
                    int statusCode,
                    std::chrono::microseconds duration);

    nlohmann::json snapshot() const;

private:
    struct RouteMetrics
    {
        uint64_t requests = 0;
        uint64_t errors = 0;
        uint64_t totalMicros = 0;
        std::deque<long long> recentMicros;
    };

    PerfMetrics() = default;

    static long long percentile95(const std::deque<long long> &samples);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, RouteMetrics> routeMetrics_;
    std::chrono::steady_clock::time_point startedAt_ = std::chrono::steady_clock::now();

    std::atomic<uint64_t> totalRequests_{0};
    std::atomic<uint64_t> totalErrors_{0};
    std::atomic<int> openConnections_{0};
    std::atomic<int> websocketClients_{0};
    std::atomic<uint64_t> websocketMessages_{0};
    std::atomic<uint64_t> websocketBroadcasts_{0};
    std::atomic<uint64_t> websocketBroadcastFanout_{0};
    std::atomic<bool> dbModeEnabled_{true};
    std::atomic<bool> perfDbAvailable_{false};
    std::atomic<size_t> memoryDatasetSize_{0};
};
