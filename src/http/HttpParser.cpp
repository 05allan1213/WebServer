#include "HttpParser.h"
#include "base/Buffer.h"
#include <algorithm>
#include <iterator>
#include <cstring>
#include "log/Log.h"

/**
 * @brief 构造函数，初始化解析状态
 */
HttpParser::HttpParser()
    : state_(HttpRequestParseState::kExpectRequestLine), chunkLeft_(0) {}

/**
 * @brief 重置解析器状态，准备解析下一个请求
 */
void HttpParser::reset()
{
    state_ = HttpRequestParseState::kExpectRequestLine;
    chunkLeft_ = 0;
    HttpRequest dummy;
    request_.swap(dummy);
}

/**
 * @brief 解析HTTP请求
 * @param buf 输入缓冲区
 * @return 解析是否成功
 *
 * 支持分段数据，自动维护状态机。
 * 支持分块传输编码的解析。
 */
bool HttpParser::parseRequest(Buffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        // --- 1. 解析请求行 ---
        if (state_ == HttpRequestParseState::kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = parseRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    buf->retrieveUntil(crlf + 2);
                    state_ = HttpRequestParseState::kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        // --- 2. 解析请求头部 ---
        else if (state_ == HttpRequestParseState::kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    // 找到冒号，解析头部字段
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // 空行，头部解析结束，判断body类型
                    auto transferEncoding = request_.getHeader("Transfer-Encoding");
                    if (transferEncoding && *transferEncoding == "chunked")
                    {
                        // 分块传输编码
                        state_ = HttpRequestParseState::kExpectChunkSize;
                    }
                    else
                    {
                        // 普通传输编码
                        state_ = HttpRequestParseState::kExpectBody;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        // --- 3. 解析普通Body（Content-Length） ---
        else if (state_ == HttpRequestParseState::kExpectBody)
        {
            if (request_.getMethod() == HttpRequest::Method::kPost || request_.getMethod() == HttpRequest::Method::kPut)
            {
                auto contentLengthOpt = request_.getHeader("Content-Length");
                if (!contentLengthOpt.has_value())
                {
                    // 没有Content-Length头部，认为解析完成
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                    continue;
                }
                size_t contentLength = 0;
                try
                {
                    contentLength = std::stoul(*contentLengthOpt);
                }
                catch (...)
                {
                    return false;
                }

                if (buf->readableBytes() >= contentLength)
                {
                    // 缓冲区中有足够的数据，解析完成
                    request_.setBody(buf->peek(), contentLength);
                    buf->retrieve(contentLength);
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                }
                else
                {
                    // 数据不足，等待更多数据
                    hasMore = false;
                }
            }
            else
            {
                // GET等请求通常没有body，解析完成
                state_ = HttpRequestParseState::kGotAll;
                hasMore = false;
            }
        }
        // --- 4. 解析分块传输编码 ---
        else if (state_ == HttpRequestParseState::kExpectChunkSize)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                // 解析分块大小（十六进制）
                std::string sizeStr(buf->peek(), crlf);
                try
                {
                    chunkLeft_ = std::stoul(sizeStr, nullptr, 16);
                }
                catch (...)
                {
                    return false;
                }
                buf->retrieveUntil(crlf + 2);

                if (chunkLeft_ == 0)
                {
                    // 最后一个分块，解析完成
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                }
                else
                {
                    state_ = HttpRequestParseState::kExpectChunkBody;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == HttpRequestParseState::kExpectChunkBody)
        {
            if (buf->readableBytes() >= chunkLeft_ + 2) // +2 for \r\n
            {
                // 读取分块数据
                request_.setBody(buf->peek(), chunkLeft_);
                buf->retrieve(chunkLeft_);
                buf->retrieve(2); // 跳过\r\n
                state_ = HttpRequestParseState::kExpectChunkSize;
            }
            else
            {
                hasMore = false;
            }
        }
        else
        {
            hasMore = false;
        }
    }
    return ok;
}

/**
 * @brief 解析请求行
 * @param begin 起始指针
 * @param end 结束指针
 * @return 是否解析成功
 *
 * 解析格式：METHOD SP URI SP HTTP-VERSION
 */
bool HttpParser::parseRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *question = std::find(start, space, '?');
            if (question != space)
            {
                request_.setPath(start, question);
                request_.setQuery(question + 1, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion(HttpRequest::Version::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion(HttpRequest::Version::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}
