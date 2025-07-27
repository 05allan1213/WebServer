#include "http/HttpRequest.h"
#include "log/Log.h"

HttpRequest::HttpRequest() : method_(Method::kInvalid), version_(Version::kUnknown), user_id_(-1) {}

void HttpRequest::setPath(const char *start, const char *end)
{
    path_.assign(start, end);
    DLOG_DEBUG << "[HttpRequest] setPath: " << path_;
}

void HttpRequest::setQuery(const char *start, const char *end)
{
    query_.assign(start, end);
    DLOG_DEBUG << "[HttpRequest] setQuery: " << query_;
}

void HttpRequest::setBody(const char *start, size_t len)
{
    body_.assign(start, len);
    DLOG_DEBUG << "[HttpRequest] setBody, 长度: " << len;
}

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
    DLOG_DEBUG << "[HttpRequest] setMethod: " << m << ", 结果: " << static_cast<int>(method_);
    return method_ != Method::kInvalid;
}

bool HttpRequest::setMethod(const std::string &m)
{
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
    DLOG_DEBUG << "[HttpRequest] setMethod: " << m << ", 结果: " << static_cast<int>(method_);
    return method_ != Method::kInvalid;
}

void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string field(start, colon);
    // 去除字段名前后的空白字符
    field.erase(0, field.find_first_not_of(" \t"));
    field.erase(field.find_last_not_of(" \t") + 1);

    // 将header的key转为小写，实现大小写不敏感
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);

    // 去除值前面的空白字符
    ++colon;
    while (colon < end && isspace(*colon))
    {
        ++colon;
    }

    std::string value(colon, end);
    // 去除值后面的空白字符
    value.erase(value.find_last_not_of(" \t\r\n") + 1);

    headers_[field] = value;
    DLOG_DEBUG << "[HttpRequest] addHeader: " << field << ": " << value;
}

std::optional<std::string> HttpRequest::getHeader(const std::string &key) const
{
    std::string lowerKey = key;
    // 转换为小写以支持大小写不敏感的查找
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

    auto it = headers_.find(lowerKey);
    if (it != headers_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

const char *HttpRequest::getMethodString() const
{
    switch (method_)
    {
    case Method::kGet:
        return "GET";
    case Method::kPost:
        return "POST";
    case Method::kHead:
        return "HEAD";
    case Method::kPut:
        return "PUT";
    case Method::kDelete:
        return "DELETE";
    default:
        return "INVALID";
    }
}

void HttpRequest::swap(HttpRequest &that)
{
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    std::swap(path_, that.path_);
    std::swap(query_, that.query_);
    std::swap(headers_, that.headers_);
    std::swap(body_, that.body_);
    std::swap(params_, that.params_);
    std::swap(user_id_, that.user_id_);
    std::swap(context_, that.context_);
}

std::optional<std::string> HttpRequest::getParam(const std::string &key) const
{
    auto it = params_.find(key);
    if (it != params_.end())
    {
        return it->second;
    }
    return std::nullopt;
}