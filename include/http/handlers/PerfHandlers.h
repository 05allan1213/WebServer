#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketHandler.h"
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <unordered_map>

class PerfConfig;

/**
 * @brief 初始化 perf 子系统
 */
void initPerfHandlers(const std::shared_ptr<PerfConfig> &config);

/**
 * @brief 配置热更新时刷新 perf 子系统
 */
void refreshPerfHandlers(const std::shared_ptr<PerfConfig> &config);

/**
 * @brief WebSocket 聊天与广播处理器
 */
class ChatWebSocketHandler : public WebSocketHandler
{
public:
    void onConnect(const TcpConnectionPtr &conn) override;
    void onMessage(const TcpConnectionPtr &conn, const std::string &message) override;
    void onClose(const TcpConnectionPtr &conn) override;

private:
    void joinRoom(const TcpConnectionPtr &conn, const std::string &room, const std::string &nickname);
    void broadcastToRoom(const std::string &room, const std::string &eventType, const nlohmann::json &payload);
    size_t cleanupExpiredLocked(const std::string &room);

    std::mutex mutex_;
    std::unordered_map<std::string, std::string> connectionRooms_;
    std::unordered_map<std::string, std::string> nicknames_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::weak_ptr<TcpConnection>>> rooms_;
};

void perfPingHandler(const HttpRequest &req, HttpResponse *resp);
void perfJsonHandler(const HttpRequest &req, HttpResponse *resp);
void perfEchoJsonHandler(const HttpRequest &req, HttpResponse *resp);
void perfItemsHandler(const HttpRequest &req, HttpResponse *resp);
void perfBatchItemsHandler(const HttpRequest &req, HttpResponse *resp);
void perfComputeHandler(const HttpRequest &req, HttpResponse *resp);
void perfFileHandler(const HttpRequest &req, HttpResponse *resp);
void perfStatsHandler(const HttpRequest &req, HttpResponse *resp);
