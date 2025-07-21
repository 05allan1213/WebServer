#pragma once
#include <string>
#include "HttpRequest.h"
#include "HttpResponse.h"

/**
 * @brief 静态资源处理器
 *
 * 用于处理静态文件请求，将URL路径映射到本地文件系统，并返回相应内容。
 * 支持处理常见的HTTP状态码（200、400、403、404、500等）。
 * 自动设置适当的MIME-Type和其他响应头。
 */
class StaticFileHandler
{
public:
    /**
     * @brief 处理静态文件请求
     *
     * @param req HTTP请求对象
     * @param resp HTTP响应对象，用于填充响应内容
     * @param baseDir 静态资源根目录，默认为"web_static"
     * @return true 表示已处理请求（无论成功与否）
     * @return false 表示未命中静态资源
     *
     * 处理流程：
     * 1. 将URL路径映射到本地文件
     * 2. 检查文件是否存在，不存在返回404
     * 3. 检查文件权限，无权限返回403
     * 4. 检查请求合法性，不合法返回400
     * 5. 读取文件内容，失败返回500
     * 6. 设置适当的响应头和内容
     */
    static bool handle(const HttpRequest &req, HttpResponse *resp, const std::string &baseDir = "web_static");
};