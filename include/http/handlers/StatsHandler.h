#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

/**
 * @brief 监控统计信息处理器
 */
void statsHandler(const HttpRequest &req, HttpResponse *resp);
