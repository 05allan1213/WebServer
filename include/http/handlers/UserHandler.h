#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

/**
 * @brief 用户注册处理函数
 */
void userRegister(const HttpRequest &req, HttpResponse *resp);

/**
 * @brief 用户登录处理函数
 */
void userLogin(const HttpRequest &req, HttpResponse *resp);
