#include "WebServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <iostream>
#include "log/Log.h"

extern void userRegister(const HttpRequest &, HttpResponse *);
extern void userLogin(const HttpRequest &, HttpResponse *);

int main()
{
    WebServer server;
    // 注册一个 /api/hello 路由
    server.addRoute("/api/hello", [](const HttpRequest &req, HttpResponse *resp)
                    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody("{\"msg\":\"Hello, Router!\"}"); });
    server.addRoute("/api/register", userRegister);
    server.addRoute("/api/login", userLogin);
    server.start();
    return 0;
}
