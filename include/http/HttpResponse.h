#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include "base/Buffer.h"
#include "log/Log.h"

/**
 * @brief HTTP响应对象,封装响应状态、头部、消息体等信息
 *
 * 用法：
 * 1. 由业务代码生成并填充
 * 2. 通过appendToBuffer序列化为HTTP响应报文
 */
class HttpResponse
{
public:
    /**
     * @brief HTTP状态码枚举
     */
    enum HttpStatusCode
    {
        kUnknown,                      // 未知状态码
        k101SwitchingProtocols = 101,  // 协议切换
        k200Ok = 200,                  // 成功
        k201Created = 201,             // 资源创建成功
        k301MovedPermanently = 301,    // 永久重定向
        k400BadRequest = 400,          // 请求错误
        k401Unauthorized = 401,        // 未认证
        k403Forbidden = 403,           // 禁止访问
        k404NotFound = 404,            // 未找到
        k409Conflict = 409,            // 资源冲突 (例如，用户名已存在)
        k500InternalServerError = 500, // 服务器错误
    };

    /**
     * @brief 构造函数
     * @param close 是否关闭连接
     */
    explicit HttpResponse(bool close);

    /**
     * @brief 设置响应状态码
     * @param code 状态码枚举
     */
    void setStatusCode(HttpStatusCode code)
    {
        statusCode_ = code;
        DLOG_DEBUG << "[HttpResponse] setStatusCode: " << static_cast<int>(code);
    }

    /**
     * @brief 获取响应状态码
     * @return 状态码枚举
     */
    HttpStatusCode getStatusCode() const { return statusCode_; }

    /**
     * @brief 设置状态消息
     * @param message 状态消息字符串
     */
    void setStatusMessage(const std::string &message)
    {
        statusMessage_ = message;
        DLOG_DEBUG << "[HttpResponse] setStatusMessage: " << message;
    }

    /**
     * @brief 设置响应头部字段
     * @param key   头部名
     * @param value 头部值
     */
    void setHeader(const std::string &key, const std::string &value) { headers_[key] = value; }

    /**
     * @brief 设置内容类型(Content-Type)
     * @param contentType 内容类型字符串
     */
    void setContentType(const std::string &contentType)
    {
        setHeader("Content-Type", contentType);
        DLOG_DEBUG << "[HttpResponse] setContentType: " << contentType;
    }
    /**
     * @brief 设置响应体
     * @param body 响应体字符串
     */
    void setBody(const std::string &body)
    {
        body_ = body;
        DLOG_DEBUG << "[HttpResponse] setBody, 长度: " << body.size();
    }
    /**
     * @brief 设置一个文件路径用于零拷贝发送
     * @param path 要发送的文件的完整路径
     */
    void setFilePath(const std::string &path) { filePath_ = path; }
    /**
     * @brief 获取用于零拷贝的文件路径
     * @return std::optional<std::string> 包含文件路径，如果未设置则为空
     */
    const std::optional<std::string> &getFilePath() const { return filePath_; }
    /**
     * @brief 获取是否需要关闭连接
     * @return true表示需要关闭
     */
    bool closeConnection() const { return closeConnection_; }

    /**
     * @brief 启用分块传输编码
     * @details 当响应体很大或长度未知时使用。
     * 启用后，将忽略Content-Length，并使用Transfer-Encoding: chunked。
     */
    void setChunkedEncoding(bool on) { chunked_ = on; }

    /**
     * @brief 序列化响应为HTTP报文,写入缓冲区
     * @param output 输出缓冲区指针
     */
    void appendToBuffer(Buffer *output) const;

    // 设置 Content-Length
    void setContentLength(size_t length);
    // 设置 Last-Modified
    void setLastModified(const std::string &time);
    // 设置 ETag
    void setETag(const std::string &etag);
    // 设置 Cache-Control
    void setCacheControl(const std::string &value);

private:
    HttpStatusCode statusCode_;                            // 响应状态码
    std::string statusMessage_;                            // 状态消息
    std::unordered_map<std::string, std::string> headers_; // 响应头部
    std::string body_;                                     // 响应体
    bool closeConnection_;                                 // 是否关闭连接
    std::optional<std::string> filePath_;                  // 文件路径,用于零拷贝发送
    bool chunked_ = false;                                 // 是否启用分块传输编码
};