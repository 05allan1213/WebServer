#include <gtest/gtest.h>
#include "base/PerfConfig.h"
#include "base/Buffer.h"
#include "http/HttpResponse.h"
#include "http/handlers/PerfHandlers.h"
#include <memory>
#include <string>
#include <yaml-cpp/yaml.h>

namespace
{
std::shared_ptr<PerfConfig> makePerfConfig(const std::string &yaml)
{
    return std::make_shared<PerfConfig>(YAML::Load(yaml));
}

std::string responseBody(const HttpResponse &response)
{
    Buffer buffer;
    response.appendToBuffer(&buffer);
    std::string serialized = buffer.retrieveAllAsString();
    size_t pos = serialized.find("\r\n\r\n");
    if (pos == std::string::npos)
    {
        return {};
    }
    return serialized.substr(pos + 4);
}
} // namespace

TEST(PerfPlatformTest, PingHandlerReturnsJsonPayload)
{
    initPerfHandlers(makePerfConfig(
        "memory_dataset_size: 16\n"
        "batch_default_size: 4\n"
        "enable_db_mode: true\n"
        "static_perf_dir: web_static/perf\n"
        "stats_poll_interval_ms: 1000\n"));

    HttpRequest request;
    request.setMethod("GET");
    request.setPath("/perf/ping", "/perf/ping" + 10);

    HttpResponse response(false);
    perfPingHandler(request, &response);

    EXPECT_EQ(response.getStatusCode(), HttpResponse::k200Ok);
    EXPECT_NE(responseBody(response).find("\"endpoint\": \"/perf/ping\""), std::string::npos);
}

TEST(PerfPlatformTest, MemoryItemsCanRoundTrip)
{
    initPerfHandlers(makePerfConfig(
        "memory_dataset_size: 8\n"
        "batch_default_size: 2\n"
        "enable_db_mode: true\n"
        "static_perf_dir: web_static/perf\n"
        "stats_poll_interval_ms: 1000\n"));

    HttpRequest postRequest;
    postRequest.setMethod("POST");
    postRequest.setPath("/perf/items/batch", "/perf/items/batch" + 17);
    std::string query = "mode=memory";
    postRequest.setQuery(query.c_str(), query.c_str() + query.size());
    std::string body = R"({"items":[{"name":"alpha","category":"frontend","score":88.5},{"name":"beta","category":"network","score":92.1}]})";
    postRequest.setBody(body.c_str(), body.size());

    HttpResponse postResponse(false);
    perfBatchItemsHandler(postRequest, &postResponse);
    EXPECT_EQ(postResponse.getStatusCode(), HttpResponse::k201Created);

    HttpRequest getRequest;
    getRequest.setMethod("GET");
    getRequest.setPath("/perf/items", "/perf/items" + 11);
    std::string getQuery = "mode=memory&limit=4";
    getRequest.setQuery(getQuery.c_str(), getQuery.c_str() + getQuery.size());

    HttpResponse getResponse(false);
    perfItemsHandler(getRequest, &getResponse);

    EXPECT_EQ(getResponse.getStatusCode(), HttpResponse::k200Ok);
    std::string payload = responseBody(getResponse);
    EXPECT_NE(payload.find("\"mode\": \"memory\""), std::string::npos);
    EXPECT_NE(payload.find("\"alpha\""), std::string::npos);
}

TEST(PerfPlatformTest, DbModeDisabledReturnsServiceUnavailable)
{
    initPerfHandlers(makePerfConfig(
        "memory_dataset_size: 8\n"
        "batch_default_size: 2\n"
        "enable_db_mode: false\n"
        "static_perf_dir: web_static/perf\n"
        "stats_poll_interval_ms: 1000\n"));

    HttpRequest request;
    request.setMethod("GET");
    request.setPath("/perf/items", "/perf/items" + 11);
    std::string query = "mode=db&limit=2";
    request.setQuery(query.c_str(), query.c_str() + query.size());

    HttpResponse response(false);
    perfItemsHandler(request, &response);

    EXPECT_EQ(response.getStatusCode(), HttpResponse::k503ServiceUnavailable);
    EXPECT_NE(responseBody(response).find("db"), std::string::npos);
}

TEST(PerfPlatformTest, FileHandlerUsesConfiguredDirectory)
{
    initPerfHandlers(makePerfConfig(
        "memory_dataset_size: 8\n"
        "batch_default_size: 2\n"
        "enable_db_mode: true\n"
        "static_perf_dir: web_static/perf\n"
        "stats_poll_interval_ms: 1000\n"));

    HttpRequest request;
    request.setMethod("GET");
    request.setPath("/perf/file/manifest.json", "/perf/file/manifest.json" + 24);
    request.setParams({{"name", "manifest.json"}});

    HttpResponse response(false);
    perfFileHandler(request, &response);

    EXPECT_EQ(response.getStatusCode(), HttpResponse::k200Ok);
    ASSERT_TRUE(response.getFilePath().has_value());
    EXPECT_NE(response.getFilePath()->find("web_static/perf/manifest.json"), std::string::npos);
}

TEST(PerfPlatformTest, StatsHandlerIncludesPerfSection)
{
    initPerfHandlers(makePerfConfig(
        "memory_dataset_size: 12\n"
        "batch_default_size: 4\n"
        "enable_db_mode: true\n"
        "static_perf_dir: web_static/perf\n"
        "stats_poll_interval_ms: 1200\n"));

    HttpRequest request;
    request.setMethod("GET");
    request.setPath("/debug/perf-stats", "/debug/perf-stats" + 17);

    HttpResponse response(false);
    perfStatsHandler(request, &response);

    EXPECT_EQ(response.getStatusCode(), HttpResponse::k200Ok);
    std::string payload = responseBody(response);
    EXPECT_NE(payload.find("\"perf\""), std::string::npos);
    EXPECT_NE(payload.find("web_static/perf"), std::string::npos);
}
