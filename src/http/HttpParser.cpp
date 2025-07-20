#include "HttpParser.h"
#include "base/Buffer.h"
#include <algorithm>
#include <iterator>
#include <cstring>

HttpParser::HttpParser() : state_(HttpRequestParseState::kExpectRequestLine) {}

void HttpParser::reset()
{
    state_ = HttpRequestParseState::kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
}

bool HttpParser::parseRequest(Buffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        if (state_ == HttpRequestParseState::kExpectRequestLine)
        {
            const char *crlf = nullptr;
            const char *peek = buf->peek();
            size_t len = buf->readableBytes();
            const char *end = peek + len;
            auto it = std::search(peek, end, "\r\n", "\r\n" + 2);
            if (it != end)
                crlf = it;
            if (crlf)
            {
                ok = parseRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    buf->retrieve(crlf + 2 - buf->peek());
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
        else if (state_ == HttpRequestParseState::kExpectHeaders)
        {
            const char *crlf = nullptr;
            const char *peek = buf->peek();
            size_t len = buf->readableBytes();
            const char *end = peek + len;
            auto it = std::search(peek, end, "\r\n", "\r\n" + 2);
            if (it != end)
                crlf = it;
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // Empty line, end of headers
                    state_ = HttpRequestParseState::kExpectBody;
                }
                buf->retrieve(crlf + 2 - buf->peek());
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == HttpRequestParseState::kExpectBody)
        {
            // For simplicity, this parser only handles requests where Content-Length is specified.
            // A more complete implementation would need to handle chunked encoding.
            if (request_.method() == HttpRequest::Method::kPost || request_.method() == HttpRequest::Method::kPut)
            {
                auto contentLengthOpt = request_.getHeader("Content-Length");
                if (!contentLengthOpt.has_value())
                {
                    // Assuming no body if Content-Length is not present
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                    continue; // Re-evaluate loop condition
                }

                size_t contentLength = 0;
                try
                {
                    contentLength = std::stoul(*contentLengthOpt);
                }
                catch (...)
                {
                    ok = false;
                    hasMore = false;
                    break;
                }

                if (buf->readableBytes() >= contentLength)
                {
                    request_.body().assign(buf->peek(), contentLength);
                    buf->retrieve(contentLength);
                    state_ = HttpRequestParseState::kGotAll;
                    hasMore = false;
                }
                else
                {
                    hasMore = false; // Need more data
                }
            }
            else
            {
                state_ = HttpRequestParseState::kGotAll;
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