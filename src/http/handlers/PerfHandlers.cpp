#include "http/handlers/PerfHandlers.h"
#include "base/Buffer.h"
#include "base/PerfConfig.h"
#include "http/StaticFileHandler.h"
#include "http/perf/PerfMetrics.h"
#include "http/perf/PerfPlatform.h"
#include "log/Log.h"
#include "net/TcpConnection.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <sys/stat.h>

using json = nlohmann::json;

namespace
{
std::string currentTimestampString()
{
    std::time_t now = std::time(nullptr);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buffer;
}

std::string trimCopy(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string jsonPrimitiveToString(const json &value)
{
    if (value.is_string())
    {
        return value.get<std::string>();
    }
    if (value.is_boolean())
    {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number_integer())
    {
        return std::to_string(value.get<long long>());
    }
    if (value.is_number_unsigned())
    {
        return std::to_string(value.get<unsigned long long>());
    }
    if (value.is_number_float())
    {
        std::ostringstream oss;
        oss << value.get<double>();
        return oss.str();
    }
    return "";
}

std::string jsonFieldToString(const json &payload, const char *key, const std::string &fallback)
{
    if (!payload.is_object())
    {
        return fallback;
    }

    auto it = payload.find(key);
    if (it == payload.end() || it->is_null())
    {
        return fallback;
    }

    std::string converted = jsonPrimitiveToString(*it);
    if (!converted.empty())
    {
        return converted;
    }
    return fallback;
}

std::string normalizeRoomName(const std::string &room)
{
    std::string trimmed = trimCopy(room);
    return trimmed.empty() ? "lobby" : trimmed;
}

std::string normalizeNickname(const std::string &nickname, const std::string &fallback)
{
    std::string trimmed = trimCopy(nickname);
    return trimmed.empty() ? fallback : trimmed;
}

std::unordered_map<std::string, std::string> parseQueryString(const std::string &query)
{
    std::unordered_map<std::string, std::string> values;
    size_t start = 0;
    while (start < query.size())
    {
        size_t eq = query.find('=', start);
        size_t amp = query.find('&', start);
        if (amp == std::string::npos)
        {
            amp = query.size();
        }

        std::string key;
        std::string value;
        if (eq != std::string::npos && eq < amp)
        {
            key = query.substr(start, eq - start);
            value = query.substr(eq + 1, amp - eq - 1);
        }
        else
        {
            key = query.substr(start, amp - start);
        }

        if (!key.empty())
        {
            values[key] = value;
        }
        start = amp + 1;
    }
    return values;
}

std::string pickMode(const HttpRequest &req)
{
    auto query = parseQueryString(req.getQuery());
    auto it = query.find("mode");
    if (it == query.end() || it->second.empty())
    {
        return "memory";
    }
    return it->second;
}

size_t parseLimit(const HttpRequest &req, size_t fallback, size_t maxValue)
{
    auto query = parseQueryString(req.getQuery());
    auto it = query.find("limit");
    if (it == query.end() || it->second.empty())
    {
        return fallback;
    }

    try
    {
        size_t value = static_cast<size_t>(std::stoul(it->second));
        return std::max<size_t>(1, std::min(maxValue, value));
    }
    catch (...)
    {
        return fallback;
    }
}

int parseLoops(const HttpRequest &req, int fallback)
{
    auto query = parseQueryString(req.getQuery());
    auto it = query.find("loops");
    if (it == query.end() || it->second.empty())
    {
        return fallback;
    }

    try
    {
        int value = std::stoi(it->second);
        return std::max(1000, std::min(250000, value));
    }
    catch (...)
    {
        return fallback;
    }
}

void setJson(HttpResponse *resp, HttpResponse::HttpStatusCode code, const json &body)
{
    resp->setStatusCode(code);
    resp->setContentType("application/json; charset=utf-8");
    resp->setBody(body.dump(2));
}

void setError(HttpResponse *resp, HttpResponse::HttpStatusCode code, const std::string &message)
{
    setJson(resp, code, json{
                            {"status", "error"},
                            {"message", message},
                        });
}

std::vector<PerfWriteItem> buildBatchPayload(const HttpRequest &req)
{
    auto perfConfig = PerfPlatform::instance().config();
    size_t defaultBatch = perfConfig ? static_cast<size_t>(perfConfig->getBatchDefaultSize()) : 16;

    if (req.getBody().empty())
    {
        std::vector<PerfWriteItem> items;
        items.reserve(defaultBatch);
        for (size_t i = 0; i < defaultBatch; ++i)
        {
            PerfWriteItem item;
            item.name = "generated-item-" + std::to_string(i + 1);
            item.category = (i % 2 == 0) ? "synthetic" : "mixed";
            item.score = 72.5 + (i % 13);
            items.push_back(std::move(item));
        }
        return items;
    }

    json parsed = json::parse(req.getBody());
    const json *itemsNode = nullptr;
    if (parsed.is_array())
    {
        itemsNode = &parsed;
    }
    else if (parsed.contains("items") && parsed["items"].is_array())
    {
        itemsNode = &parsed["items"];
    }
    else
    {
        throw std::invalid_argument("请求体必须是数组，或包含 items 数组字段");
    }

    std::vector<PerfWriteItem> items;
    items.reserve(itemsNode->size());
    for (const auto &entry : *itemsNode)
    {
        PerfWriteItem item;
        item.name = entry.value("name", "");
        item.category = entry.value("category", "mixed");
        item.score = entry.value("score", 0.0);
        if (item.name.empty())
        {
            throw std::invalid_argument("批量写入项缺少 name");
        }
        items.push_back(std::move(item));
    }
    return items;
}

json itemToJson(const PerfItem &item)
{
    return json{
        {"id", item.id},
        {"name", item.name},
        {"category", item.category},
        {"score", item.score},
        {"source", item.source},
        {"created_at", item.createdAt},
    };
}
} // namespace

void initPerfHandlers(const std::shared_ptr<PerfConfig> &config)
{
    PerfPlatform::instance().initialize(config);
}

void refreshPerfHandlers(const std::shared_ptr<PerfConfig> &config)
{
    PerfPlatform::instance().refresh(config);
}

void ChatWebSocketHandler::onConnect(const TcpConnectionPtr &conn)
{
    PerfMetrics::instance().onWebSocketConnected();
    std::lock_guard<std::mutex> lock(mutex_);
    connectionRooms_[conn->name()] = "lobby";
    nicknames_[conn->name()] = conn->name();
    rooms_["lobby"][conn->name()] = conn;
}

void ChatWebSocketHandler::onMessage(const TcpConnectionPtr &conn, const std::string &message)
{
    PerfMetrics::instance().onWebSocketMessage();

    json payload;
    bool parsed = true;
    try
    {
        payload = json::parse(message);
    }
    catch (...)
    {
        parsed = false;
    }

    std::string room = "lobby";
    std::string nickname = conn->name();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto roomIt = connectionRooms_.find(conn->name());
        if (roomIt != connectionRooms_.end())
        {
            room = roomIt->second;
        }
        auto nickIt = nicknames_.find(conn->name());
        if (nickIt != nicknames_.end())
        {
            nickname = nickIt->second;
        }
    }

    std::string text;
    std::string type = "message";
    if (!parsed)
    {
        text = trimCopy(message);
    }
    else if (!payload.is_object())
    {
        text = trimCopy(jsonPrimitiveToString(payload));
    }
    else
    {
        type = trimCopy(jsonFieldToString(payload, "type", "message"));
        if (type.empty())
        {
            type = "message";
        }
    }

    if (parsed && payload.is_object() && type == "join")
    {
        std::string nextRoom = normalizeRoomName(jsonFieldToString(payload, "room", room));
        std::string nextNickname = normalizeNickname(jsonFieldToString(payload, "nickname", nickname), nickname);
        joinRoom(conn, nextRoom, nextNickname);
        conn->sendWebSocket(json{
                                {"type", "joined"},
                                {"room", nextRoom},
                                {"nickname", nextNickname},
                            }
                                .dump());
        return;
    }

    if (parsed && payload.is_object())
    {
        text = trimCopy(jsonFieldToString(payload, "text", jsonFieldToString(payload, "message", "")));
    }

    if (text.empty())
    {
        conn->sendWebSocket(json{
                                {"type", "error"},
                                {"message", "消息内容不能为空"},
                            }
                                .dump());
        return;
    }

    broadcastToRoom(room, "message", json{
                                        {"room", room},
                                        {"sender", nickname},
                                        {"text", text},
                                        {"timestamp", currentTimestampString()},
                                    });
}

void ChatWebSocketHandler::onClose(const TcpConnectionPtr &conn)
{
    std::string room;
    std::string nickname = conn->name();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto roomIt = connectionRooms_.find(conn->name());
        if (roomIt != connectionRooms_.end())
        {
            room = roomIt->second;
            connectionRooms_.erase(roomIt);
        }
        auto nickIt = nicknames_.find(conn->name());
        if (nickIt != nicknames_.end())
        {
            nickname = nickIt->second;
            nicknames_.erase(nickIt);
        }
        if (!room.empty())
        {
            auto roomIt2 = rooms_.find(room);
            if (roomIt2 != rooms_.end())
            {
                roomIt2->second.erase(conn->name());
            }
        }
    }

    PerfMetrics::instance().onWebSocketDisconnected();
    if (!room.empty())
    {
        broadcastToRoom(room, "presence", json{
                                              {"room", room},
                                              {"message", nickname + " 离开了房间"},
                                          });
    }
}

void ChatWebSocketHandler::joinRoom(const TcpConnectionPtr &conn, const std::string &room, const std::string &nickname)
{
    std::string previousRoom;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto previous = connectionRooms_.find(conn->name());
        if (previous != connectionRooms_.end())
        {
            previousRoom = previous->second;
            if (!previousRoom.empty() && rooms_.count(previousRoom))
            {
                rooms_[previousRoom].erase(conn->name());
            }
        }

        connectionRooms_[conn->name()] = room;
        nicknames_[conn->name()] = nickname;
        rooms_[room][conn->name()] = conn;
    }

    if (!previousRoom.empty() && previousRoom != room)
    {
        broadcastToRoom(previousRoom, "presence", json{
                                                      {"room", previousRoom},
                                                      {"message", nickname + " 切换到了 " + room},
                                                  });
    }
    broadcastToRoom(room, "presence", json{
                                          {"room", room},
                                          {"message", nickname + " 加入了房间"},
                                      });
}

void ChatWebSocketHandler::broadcastToRoom(const std::string &room, const std::string &eventType, const json &payload)
{
    std::vector<TcpConnectionPtr> targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto roomIt = rooms_.find(room);
        if (roomIt == rooms_.end())
        {
            return;
        }

        cleanupExpiredLocked(room);
        for (const auto &entry : roomIt->second)
        {
            auto target = entry.second.lock();
            if (target)
            {
                targets.push_back(std::move(target));
            }
        }
    }

    json envelope = payload;
    envelope["type"] = eventType;
    envelope["audience"] = targets.size();

    std::string frame = envelope.dump();
    for (const auto &target : targets)
    {
        target->sendWebSocket(frame);
    }
    PerfMetrics::instance().onWebSocketBroadcast(targets.size());
}

size_t ChatWebSocketHandler::cleanupExpiredLocked(const std::string &room)
{
    auto roomIt = rooms_.find(room);
    if (roomIt == rooms_.end())
    {
        return 0;
    }

    size_t removed = 0;
    for (auto it = roomIt->second.begin(); it != roomIt->second.end();)
    {
        if (it->second.expired())
        {
            connectionRooms_.erase(it->first);
            nicknames_.erase(it->first);
            it = roomIt->second.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

void perfPingHandler(const HttpRequest &req, HttpResponse *resp)
{
    setJson(resp, HttpResponse::k200Ok, json{
                                             {"status", "ok"},
                                             {"endpoint", "/perf/ping"},
                                             {"mode", pickMode(req)},
                                             {"timestamp", currentTimestampString()},
                                             {"message", "perf ping ready"},
                                         });
}

void perfJsonHandler(const HttpRequest &req, HttpResponse *resp)
{
    auto stats = PerfMetrics::instance().snapshot();
    setJson(resp, HttpResponse::k200Ok, json{
                                             {"status", "ok"},
                                             {"endpoint", "/perf/json"},
                                             {"mode", pickMode(req)},
                                             {"summary",
                                              {
                                                  {"service", "WebServer Performance Console"},
                                                  {"transport", "HTTP/1.1"},
                                                  {"workload", {"json", "compute", "files", "chat", "db"}},
                                              }},
                                             {"cards",
                                              {
                                                  {{"title", "活跃连接"}, {"value", stats["service"]["open_connections"]}},
                                                  {{"title", "WebSocket 在线"}, {"value", stats["service"]["websocket_clients"]}},
                                                  {{"title", "累计请求"}, {"value", stats["service"]["total_requests"]}},
                                              }},
                                             {"timeline",
                                              {
                                                  {{"step", "accept"}, {"cost_us", 18}},
                                                  {{"step", "parse"}, {"cost_us", 27}},
                                                  {{"step", "dispatch"}, {"cost_us", 12}},
                                                  {{"step", "response"}, {"cost_us", 31}},
                                              }},
                                         });
}

void perfEchoJsonHandler(const HttpRequest &req, HttpResponse *resp)
{
    try
    {
        json parsed = req.getBody().empty() ? json::object() : json::parse(req.getBody());
        setJson(resp, HttpResponse::k200Ok, json{
                                                 {"status", "ok"},
                                                 {"endpoint", "/perf/echo-json"},
                                                 {"received_bytes", req.getBody().size()},
                                                 {"received", parsed},
                                                 {"normalized",
                                                  {
                                                      {"keys", parsed.is_object() ? parsed.size() : 0},
                                                      {"type", parsed.type_name()},
                                                      {"echoed_at", currentTimestampString()},
                                                  }},
                                             });
    }
    catch (const std::exception &e)
    {
        setError(resp, HttpResponse::k400BadRequest, e.what());
    }
}

void perfItemsHandler(const HttpRequest &req, HttpResponse *resp)
{
    std::string error;
    bool usedDb = false;
    auto repository = PerfPlatform::instance().selectRepository(pickMode(req), &usedDb, &error);
    if (!repository)
    {
        setError(resp, HttpResponse::k503ServiceUnavailable, error);
        return;
    }

    auto items = repository->listItems(parseLimit(req, 12, 200), &error);
    PerfMetrics::instance().setPerfDbAvailable(PerfPlatform::instance().dbRepository()->available());
    if (!error.empty())
    {
        setError(resp, usedDb ? HttpResponse::k503ServiceUnavailable : HttpResponse::k500InternalServerError, error);
        return;
    }

    json data = json::array();
    for (const auto &item : items)
    {
        data.push_back(itemToJson(item));
    }

    setJson(resp, HttpResponse::k200Ok, json{
                                             {"status", "ok"},
                                             {"mode", repository->modeName()},
                                             {"count", data.size()},
                                             {"items", data},
                                         });
}

void perfBatchItemsHandler(const HttpRequest &req, HttpResponse *resp)
{
    std::string error;
    bool usedDb = false;
    auto repository = PerfPlatform::instance().selectRepository(pickMode(req), &usedDb, &error);
    if (!repository)
    {
        setError(resp, HttpResponse::k503ServiceUnavailable, error);
        return;
    }

    try
    {
        auto items = buildBatchPayload(req);
        size_t inserted = 0;
        if (!repository->batchInsert(items, &inserted, &error))
        {
            setError(resp, usedDb ? HttpResponse::k503ServiceUnavailable : HttpResponse::k500InternalServerError, error);
            return;
        }

        PerfMetrics::instance().setPerfDbAvailable(PerfPlatform::instance().dbRepository()->available());
        setJson(resp, HttpResponse::k201Created, json{
                                                      {"status", "ok"},
                                                      {"mode", repository->modeName()},
                                                      {"inserted", inserted},
                                                  });
    }
    catch (const std::exception &e)
    {
        setError(resp, HttpResponse::k400BadRequest, e.what());
    }
}

void perfComputeHandler(const HttpRequest &req, HttpResponse *resp)
{
    int loops = parseLoops(req, 50000);
    double accumulator = 0.0;
    for (int i = 1; i <= loops; ++i)
    {
        accumulator += std::sin(i * 0.13) * std::cos(i * 0.07) + std::sqrt((i % 97) + 1.0);
    }

    setJson(resp, HttpResponse::k200Ok, json{
                                             {"status", "ok"},
                                             {"loops", loops},
                                             {"checksum", accumulator},
                                             {"workload", "cpu-bound"},
                                         });
}

void perfFileHandler(const HttpRequest &req, HttpResponse *resp)
{
    std::string relative = req.getParam("name").value_or("manifest.json");
    if (!relative.empty() && relative.front() != '/')
    {
        relative.insert(relative.begin(), '/');
    }

    HttpRequest rewritten = req;
    rewritten.setPath(relative.c_str(), relative.c_str() + relative.size());

    const std::string baseDir = PerfPlatform::instance().staticPerfDir();
    std::string filePath = baseDir + relative;

    struct stat st;
    if (::stat(filePath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/octet-stream");
        resp->setHeader("X-Perf-File", relative);
        resp->setHeader("X-Perf-File-Size", std::to_string(static_cast<long long>(st.st_size)));
        resp->setContentLength(st.st_size);
        if (req.getMethod() == HttpRequest::Method::kGet)
        {
            resp->setFilePath(filePath);
        }
        return;
    }

    StaticFileHandler::handle(rewritten, resp, baseDir);
}

void perfStatsHandler(const HttpRequest &req, HttpResponse *resp)
{
    (void)req;
    auto payload = PerfMetrics::instance().snapshot();
    auto perfConfig = PerfPlatform::instance().config();

    payload["buffer"]["active_count"] = Buffer::getActiveBuffers();
    payload["buffer"]["pool_memory_bytes"] = Buffer::getPoolMemory();
    payload["buffer"]["heap_memory_bytes"] = Buffer::getHeapMemory();
    payload["buffer"]["resize_count"] = Buffer::getResizeCount();
    payload["perf"]["static_dir"] = PerfPlatform::instance().staticPerfDir();
    payload["perf"]["stats_poll_interval_ms"] = perfConfig ? perfConfig->getStatsPollIntervalMs() : 1000;
    payload["perf"]["batch_default_size"] = perfConfig ? perfConfig->getBatchDefaultSize() : 16;
    payload["perf"]["db_mode_enabled"] = perfConfig ? perfConfig->isDbModeEnabled() : true;
    payload["perf"]["sample_routes"] = json::array(
        {"/perf/ping", "/perf/json", "/perf/items?mode=memory", "/perf/items?mode=db", "/perf/file/manifest.json"});

    setJson(resp, HttpResponse::k200Ok, payload);
}
