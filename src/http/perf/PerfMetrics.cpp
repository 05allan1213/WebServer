#include "http/perf/PerfMetrics.h"
#include <algorithm>

PerfMetrics &PerfMetrics::instance()
{
    static PerfMetrics metrics;
    return metrics;
}

void PerfMetrics::setDbModeEnabled(bool enabled)
{
    dbModeEnabled_.store(enabled, std::memory_order_relaxed);
}

void PerfMetrics::setPerfDbAvailable(bool available)
{
    perfDbAvailable_.store(available, std::memory_order_relaxed);
}

void PerfMetrics::setMemoryDatasetSize(size_t size)
{
    memoryDatasetSize_.store(size, std::memory_order_relaxed);
}

void PerfMetrics::onTcpConnectionOpened()
{
    openConnections_.fetch_add(1, std::memory_order_relaxed);
}

void PerfMetrics::onTcpConnectionClosed()
{
    int current = openConnections_.load(std::memory_order_relaxed);
    while (current > 0 &&
           !openConnections_.compare_exchange_weak(current, current - 1, std::memory_order_relaxed))
    {
    }
}

void PerfMetrics::onWebSocketConnected()
{
    websocketClients_.fetch_add(1, std::memory_order_relaxed);
}

void PerfMetrics::onWebSocketDisconnected()
{
    int current = websocketClients_.load(std::memory_order_relaxed);
    while (current > 0 &&
           !websocketClients_.compare_exchange_weak(current, current - 1, std::memory_order_relaxed))
    {
    }
}

void PerfMetrics::onWebSocketMessage()
{
    websocketMessages_.fetch_add(1, std::memory_order_relaxed);
}

void PerfMetrics::onWebSocketBroadcast(size_t fanout)
{
    websocketBroadcasts_.fetch_add(1, std::memory_order_relaxed);
    websocketBroadcastFanout_.fetch_add(fanout, std::memory_order_relaxed);
}

void PerfMetrics::recordHttp(const std::string &path,
                             int statusCode,
                             std::chrono::microseconds duration)
{
    totalRequests_.fetch_add(1, std::memory_order_relaxed);
    if (statusCode >= 400)
    {
        totalErrors_.fetch_add(1, std::memory_order_relaxed);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto &route = routeMetrics_[path];
    route.requests++;
    if (statusCode >= 400)
    {
        route.errors++;
    }
    route.totalMicros += static_cast<uint64_t>(duration.count());
    route.recentMicros.push_back(duration.count());
    if (route.recentMicros.size() > 128)
    {
        route.recentMicros.pop_front();
    }
}

nlohmann::json PerfMetrics::snapshot() const
{
    nlohmann::json payload;
    payload["service"]["uptime_seconds"] =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startedAt_).count();
    payload["service"]["total_requests"] = totalRequests_.load(std::memory_order_relaxed);
    payload["service"]["total_errors"] = totalErrors_.load(std::memory_order_relaxed);
    payload["service"]["open_connections"] = openConnections_.load(std::memory_order_relaxed);
    payload["service"]["websocket_clients"] = websocketClients_.load(std::memory_order_relaxed);
    payload["service"]["websocket_messages"] = websocketMessages_.load(std::memory_order_relaxed);
    payload["service"]["websocket_broadcasts"] = websocketBroadcasts_.load(std::memory_order_relaxed);
    payload["service"]["websocket_broadcast_fanout"] = websocketBroadcastFanout_.load(std::memory_order_relaxed);
    payload["service"]["db_mode_enabled"] = dbModeEnabled_.load(std::memory_order_relaxed);
    payload["service"]["db_mode_available"] = perfDbAvailable_.load(std::memory_order_relaxed);
    payload["service"]["memory_dataset_size"] = memoryDatasetSize_.load(std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json routes = nlohmann::json::array();
    for (const auto &entry : routeMetrics_)
    {
        const auto &route = entry.second;
        nlohmann::json item;
        item["path"] = entry.first;
        item["requests"] = route.requests;
        item["errors"] = route.errors;
        item["avg_us"] = route.requests == 0 ? 0 : route.totalMicros / route.requests;
        item["p95_us"] = percentile95(route.recentMicros);
        routes.push_back(std::move(item));
    }

    std::sort(routes.begin(), routes.end(), [](const nlohmann::json &lhs, const nlohmann::json &rhs)
              { return lhs.value("requests", 0ULL) > rhs.value("requests", 0ULL); });
    payload["routes"] = std::move(routes);
    return payload;
}

long long PerfMetrics::percentile95(const std::deque<long long> &samples)
{
    if (samples.empty())
    {
        return 0;
    }

    std::vector<long long> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    size_t index = static_cast<size_t>((sorted.size() - 1) * 0.95);
    return sorted[index];
}
