#include "http/handlers/Middleware.h"
#include "http/handlers/AuthHandler.h"
#include "http/perf/PerfMetrics.h"
#include "log/LogManager.h"
#include <chrono>

void loggingMiddleware(const HttpRequest &req, HttpResponse *resp, Next next)
{
    auto start = std::chrono::high_resolution_clock::now();
    DLOG_INFO << "--> " << req.getMethodString() << " " << req.getPath();

    next();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    PerfMetrics::instance().recordHttp(req.getPath(), resp->getStatusCode(), duration);
    DLOG_INFO << "<-- " << req.getMethodString() << " " << req.getPath()
              << " " << resp->getStatusCode() << " " << duration.count() << "us";
}

void authMiddleware(const HttpRequest &req, HttpResponse *resp, Next next)
{
    int user_id = -1;
    if (checkAuth(req, user_id))
    {
        const_cast<HttpRequest &>(req).setUserId(user_id);
        DLOG_INFO << "[Auth] 认证成功, user_id: " << user_id;
        next();
    }
    else
    {
        DLOG_WARN << "[Auth] 认证失败, 路径: " << req.getPath();
        resp->setStatusCode(HttpResponse::k403Forbidden);
        resp->setBody("{\"error\":\"Forbidden\"}");
        resp->setContentType("application/json");
    }
}
