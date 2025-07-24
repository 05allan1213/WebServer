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
    char buf[32];
    // 1. 添加状态行
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    output->append(buf, strlen(buf));
    output->append(statusMessage_.c_str(), statusMessage_.size());
    output->append("\r\n", 2);

    // 2. 添加头部
    if (chunked_)
    {
        output->append("Transfer-Encoding: chunked\r\n", 25);
    }
    else
    {
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf, strlen(buf));
    }

    if (closeConnection_)
    {
        output->append("Connection: close\r\n", 17);
    }
    else
    {
        output->append("Connection: Keep-Alive\r\n", 21);
    }

    for (const auto &header : headers_)
    {
        output->append(header.first.c_str(), header.first.size());
        output->append(": ", 2);
        output->append(header.second.c_str(), header.second.size());
        output->append("\r\n", 2);
    }

    // 头部和Body之间的空行
    output->append("\r\n", 2);

    // 3. 添加Body
    if (chunked_)
    {
        if (!body_.empty())
        {
            snprintf(buf, sizeof(buf), "%zx\r\n", body_.size());
            output->append(buf, strlen(buf));
            output->append(body_.c_str(), body_.size());
            output->append("\r\n", 2);
        }
        // 结束块
        output->append("0\r\n\r\n", 5);
    }
    else
    {
        output->append(body_.c_str(), body_.size());
    }
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