#include "HttpParser.h"
#include "base/Buffer.h"
#include <algorithm>
#include <iterator>
#include <cstring>
#include "log/Log.h"

HttpParser::HttpParser()
    : state_(HttpRequestParseState::kExpectRequestLine), chunkLeft_(0) {}

void HttpParser::reset()
{
    state_ = HttpRequestParseState::kExpectRequestLine;
    chunkLeft_ = 0;
    HttpRequest dummy;
    request_.swap(dummy);
}

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
        // --- 2. 解析请求头 ---
        else if (state_ == HttpRequestParseState::kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // 空行，头部解析结束，判断body类型
                    auto transferEncoding = request_.getHeader("Transfer-Encoding");
                    if (transferEncoding && *transferEncoding == "chunked")
                    {
                        state_ = HttpRequestParseState::kExpectChunkSize;
                    }
                    else
                    {
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
        // --- 3. 解析普通Body (Content-Length) ---
        else if (state_ == HttpRequestParseState::kExpectBody)
        {
            if (request_.getMethod() == HttpRequest::Method::kPost || request_.getMethod() == HttpRequest::Method::kPut)
            {
                auto contentLengthOpt = request_.getHeader("Content-Length");
                if (!contentLengthOpt.has_value())
                {
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
                    request_.getBody().assign(buf->peek(), contentLength);
                    buf->retrieve(contentLength);
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                state_ = HttpRequestParseState::kGotAll;
                hasMore = false;
            }
        }
        // --- 4. 解析分块大小 ---
        else if (state_ == HttpRequestParseState::kExpectChunkSize)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                std::string size_str(buf->peek(), crlf);
                try
                {
                    chunkLeft_ = std::stoul(size_str, nullptr, 16);
                }
                catch (...)
                {
                    return false; // 无效的16进制大小
                }

                buf->retrieveUntil(crlf + 2);

                if (chunkLeft_ == 0)
                {
                    state_ = HttpRequestParseState::kExpectLastChunk;
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
        // --- 5. 解析分块数据 ---
        else if (state_ == HttpRequestParseState::kExpectChunkBody)
        {
            if (buf->readableBytes() >= chunkLeft_)
            {
                request_.getBody().append(buf->peek(), chunkLeft_);
                buf->retrieve(chunkLeft_);
                chunkLeft_ = 0;
                state_ = HttpRequestParseState::kExpectChunkFooter;
            }
            else
            {
                hasMore = false;
            }
        }
        // --- 6. 消费分块数据后的CRLF ---
        else if (state_ == HttpRequestParseState::kExpectChunkFooter)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                buf->retrieveUntil(crlf + 2);
                state_ = HttpRequestParseState::kExpectChunkSize;
            }
            else
            {
                hasMore = false;
            }
        }
        // --- 7. 处理最后的空分块 ---
        else if (state_ == HttpRequestParseState::kExpectLastChunk)
        {
            const char *crlf = buf->findCRLF();
            if (crlf && crlf == buf->peek()) // 必须是空行
            {
                buf->retrieveUntil(crlf + 2);
                state_ = HttpRequestParseState::kGotAll;
                hasMore = false;
            }
            else
            {
                hasMore = false;
            }
        }

        if (state_ == HttpRequestParseState::kGotAll)
        {
            break;
        }
    }
    return ok;
}

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
