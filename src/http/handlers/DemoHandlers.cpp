#include "http/handlers/DemoHandlers.h"
#include "net/TcpConnection.h"
#include "log/LogManager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void EchoWebSocketHandler::onConnect(const TcpConnectionPtr &conn)
{
    DLOG_INFO << "[WebSocket][DEMO] Echo handler new connection: " << conn->peerAddress().toIpPort();
}

void EchoWebSocketHandler::onMessage(const TcpConnectionPtr &conn, const std::string &message)
{
    DLOG_INFO << "[WebSocket][DEMO] Echo handler received message: '" << message << "' from " << conn->peerAddress().toIpPort();
    conn->sendWebSocket(message);
}

void EchoWebSocketHandler::onClose(const TcpConnectionPtr &conn)
{
    DLOG_INFO << "[WebSocket][DEMO] Echo handler connection closed: " << conn->peerAddress().toIpPort();
}

void demoRouteParamsHandler(const HttpRequest &req, HttpResponse *resp)
{
    json result;
    result["message"] = "Advanced routing works!";
    result["userId"] = req.getParam("id").value_or("not found");
    result["postId"] = req.getParam("postId").value_or("not found");

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(result.dump(4));
}

void demoProtectedHandler(const HttpRequest &req, HttpResponse *resp)
{
    json profile;
    profile["user_id"] = req.getUserId();
    profile["username"] = "test_user";
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(profile.dump());
}
