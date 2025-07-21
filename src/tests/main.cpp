#include "WebServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <iostream>
#include "log/Log.h"

extern void userRegister(const HttpRequest &, HttpResponse *);
extern void userLogin(const HttpRequest &, HttpResponse *);

int main()
{
    DLOG_INFO << "main() 启动";
    WebServer server;
    // 注册一个 /api/hello 路由
    server.addRoute("/api/hello", [](const HttpRequest &req, HttpResponse *resp)
                    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody("{\"msg\":\"Hello, Router!\"}"); });
    // 自定义404处理
    server.setNotFoundHandler([](const HttpRequest &req, HttpResponse *resp)
                              {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"Route Not Found\"}"); });
    server.addRoute("/api/register", userRegister);
    server.addRoute("/api/login", userLogin);
    server.start();
    return 0;
}
