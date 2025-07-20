#pragma once

#include "HttpRequest.h"
#include <memory>

// 前向声明 Buffer 类
class Buffer;

class HttpParser
{
public:
    enum class HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpParser();

    // 解析请求，返回是否成功
    // 如果返回 false，说明协议格式错误，应该关闭连接
    bool parseRequest(Buffer *buf);

    // 是否解析完毕
    bool gotAll() const { return state_ == HttpRequestParseState::kGotAll; }

    // 重置解析器状态，以便处理下一个请求（HTTP/1.1 Keep-Alive）
    void reset();

    // 获取解析后的请求对象
    const HttpRequest &request() const { return request_; }
    HttpRequest &request() { return request_; }

private:
    // 解析请求行
    bool parseRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_;
    HttpRequest request_;
};