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

void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string field(start, colon);
    // Trim leading and trailing spaces from field
    field.erase(0, field.find_first_not_of(" \t"));
    field.erase(field.find_last_not_of(" \t") + 1);
    // **
    // * 将header的key转为小写，实现大小写不敏感
    // **
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);

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
    DLOG_DEBUG << "[HttpRequest] addHeader: '" << field << "' = '" << value << "'";
}

std::optional<std::string> HttpRequest::getHeader(const std::string &field) const
{
    std::string lower_field = field;
    // **
    // * 将要查找的key也转为小写
    // **
    std::transform(lower_field.begin(), lower_field.end(), lower_field.begin(), ::tolower);
    auto it = headers_.find(lower_field);
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
    case Method::kPut:
        return "PUT";
    case Method::kDelete:
        return "DELETE";
    case Method::kHead:
        return "HEAD";
    default:
        return "INVALID";
    }
}

void HttpRequest::swap(HttpRequest &that)
{
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    body_.swap(that.body_);
    headers_.swap(that.headers_);
    params_.swap(that.params_);
    std::swap(user_id_, that.user_id_);
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