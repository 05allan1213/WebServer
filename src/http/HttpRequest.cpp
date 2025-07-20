#include "http/HttpRequest.h"
#include <algorithm>
#include <cctype>

bool HttpRequest::setMethod(const char *start, const char *end)
{
    std::string m(start, end);
    if (m == "GET")
    {
        method_ = Method::kGet;
    }
    else if (m == "POST")
    {
        method_ = Method::kPost;
    }
    else if (m == "HEAD")
    {
        method_ = Method::kHead;
    }
    else if (m == "PUT")
    {
        method_ = Method::kPut;
    }
    else if (m == "DELETE")
    {
        method_ = Method::kDelete;
    }
    else
    {
        method_ = Method::kInvalid;
    }
    return method_ != Method::kInvalid;
}

void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string field(start, colon);
    // Trim leading and trailing spaces from field
    field.erase(0, field.find_first_not_of(" \t"));
    field.erase(field.find_last_not_of(" \t") + 1);

    // Trim leading spaces from value
    ++colon;
    while (colon < end && isspace(*colon))
    {
        ++colon;
    }
    std::string value(colon, end);
    // Trim trailing spaces from value
    value.erase(value.find_last_not_of(" \t") + 1);

    headers_[field] = value;
}

std::optional<std::string> HttpRequest::getHeader(const std::string &field) const
{
    auto it = headers_.find(field);
    if (it != headers_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void HttpRequest::swap(HttpRequest &that)
{
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    body_.swap(that.body_);
    headers_.swap(that.headers_);
}