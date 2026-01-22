#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

/**
 * @brief JWT认证检查函数
 * @param req HTTP请求对象
 * @param user_id 输出参数，用户ID
 * @return 认证是否成功
 */
bool checkAuth(const HttpRequest &req, int &user_id);
