#include "http/HttpResponse.h"
#include <cstdio>
#include "log/Log.h"

/**
 * @brief 构造函数，初始化响应对象
 * @param close 是否关闭连接标志
 */
HttpResponse::HttpResponse(bool close)
    : statusCode_(kUnknown), closeConnection_(close) {}

/**
 * @brief 将HTTP响应序列化到缓冲区
 *
 * 按照HTTP协议格式，将响应状态行、头部、空行、消息体依次写入缓冲区
 *
 * @param output 输出缓冲区
 */
void HttpResponse::appendToBuffer(Buffer *output) const
{
    DLOG_INFO << "[HttpResponse] appendToBuffer, 状态码: " << statusCode_ << ", 消息: " << statusMessage_ << ", body长度: " << body_.size();
    // 1. 添加状态行：HTTP/1.1 状态码 状态消息\r\n
    char buf[32];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    output->append(buf, strlen(buf));
    output->append(statusMessage_.c_str(), statusMessage_.size());
    output->append("\r\n", 2);

    // 2. 添加Connection头部
    if (closeConnection_)
    {
        // 如果需要关闭连接，添加"Connection: close"
        output->append("Connection: close\r\n", strlen("Connection: close\r\n"));
    }
    else
    {
        // 否则添加Content-Length和"Connection: Keep-Alive"
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf, strlen(buf));
        output->append("Connection: Keep-Alive\r\n", strlen("Connection: Keep-Alive\r\n"));
    }

    // 3. 添加其他自定义头部
    for (const auto &header : headers_)
    {
        output->append(header.first.c_str(), header.first.size());
        output->append(": ", 2);
        output->append(header.second.c_str(), header.second.size());
        output->append("\r\n", 2);
    }

    // 4. 添加空行，分隔头部和消息体
    output->append("\r\n", 2);

    // 5. 添加消息体
    output->append(body_.c_str(), body_.size());
}

/**
 * @brief 设置Content-Length头部
 * @param length 内容长度
 */
void HttpResponse::setContentLength(size_t length)
{
    setHeader("Content-Length", std::to_string(length));
}

/**
 * @brief 设置Last-Modified头部，用于缓存控制
 * @param time 最后修改时间字符串
 */
void HttpResponse::setLastModified(const std::string &time)
{
    setHeader("Last-Modified", time);
}

/**
 * @brief 设置ETag头部，用于缓存验证
 * @param etag ETag值
 */
void HttpResponse::setETag(const std::string &etag)
{
    setHeader("ETag", etag);
}

/**
 * @brief 设置Cache-Control头部，控制缓存行为
 * @param value 缓存控制指令
 */
void HttpResponse::setCacheControl(const std::string &value)
{
    setHeader("Cache-Control", value);
}