#pragma once

#include <string>
#include <unordered_map>
#include <optional>

class HttpRequest
{
public:
    enum class Method
    {
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete
    };
    enum class Version
    {
        kUnknown,
        kHttp10,
        kHttp11
    };

    HttpRequest() : method_(Method::kInvalid), version_(Version::kUnknown) {}

    // Setters for parser
    bool setMethod(const char *start, const char *end);
    void setPath(const char *start, const char *end) { path_.assign(start, end); }
    void setQuery(const char *start, const char *end) { query_.assign(start, end); }
    void setVersion(Version version) { version_ = version; }
    void addHeader(const char *start, const char *colon, const char *end);

    // Getters for user
    Method method() const { return method_; }
    const std::string &path() const { return path_; }
    const std::string &query() const { return query_; }
    Version version() const { return version_; }
    const std::unordered_map<std::string, std::string> &headers() const { return headers_; }
    std::optional<std::string> getHeader(const std::string &field) const;
    const std::string &body() const { return body_; }
    std::string &body() { return body_; }

    void swap(HttpRequest &that);

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};