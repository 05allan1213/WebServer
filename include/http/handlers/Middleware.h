#pragma once

#include "net/Callbacks.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

/**
 * @brief 日志中间件
 */
void loggingMiddleware(const HttpRequest &req, HttpResponse *resp, Next next);

/**
 * @brief 认证中间件
 */
void authMiddleware(const HttpRequest &req, HttpResponse *resp, Next next);
