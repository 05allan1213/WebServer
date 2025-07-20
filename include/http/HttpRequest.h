#pragma once

#include <string>
#include <unordered_map>
#include <optional>

/**
 * @brief HTTP请求对象,封装请求行、头部、消息体等信息
 *
 * 用法：
 * 1. 由HttpParser解析生成
 * 2. 提供方法获取请求方法、路径、参数、头部、体等
 */
class HttpRequest
{
public:
    /**
     * @brief HTTP方法枚举
     */
    enum class Method
    {
        kInvalid, // 无效方法
        kGet,     // GET
        kPost,    // POST
        kHead,    // HEAD
        kPut,     // PUT
        kDelete   // DELETE
    };

    /**
     * @brief HTTP协议版本枚举
     */
    enum class Version
    {
        kUnknown, // 未知版本
        kHttp10,  // HTTP/1.0
        kHttp11   // HTTP/1.1
    };

    /**
     * @brief 构造函数,初始化为无效请求
     */
    HttpRequest();

    /**
     * @brief 设置HTTP方法
     * @param start 方法起始指针
     * @param end   方法结束指针
     * @return 是否设置成功
     */
    bool setMethod(const char *start, const char *end);

    /**
     * @brief 获取HTTP方法
     * @return Method枚举值
     */
    Method method() const { return method_; }

    /**
     * @brief 设置请求路径
     * @param start 路径起始指针
     * @param end   路径结束指针
     */
    void setPath(const char *start, const char *end);

    /**
     * @brief 获取请求路径
     * @return 路径字符串
     */
    const std::string &path() const { return path_; }

    /**
     * @brief 设置查询参数
     * @param start 参数起始指针
     * @param end   参数结束指针
     */
    void setQuery(const char *start, const char *end);

    /**
     * @brief 获取查询参数
     * @return 参数字符串
     */
    const std::string &query() const { return query_; }

    /**
     * @brief 设置HTTP协议版本
     * @param version 版本枚举
     */
    void setVersion(Version version) { version_ = version; }

    /**
     * @brief 获取HTTP协议版本
     * @return Version枚举值
     */
    Version version() const { return version_; }

    /**
     * @brief 添加请求头部
     * @param start 头部名起始指针
     * @param colon 冒号位置指针
     * @param end   头部值结束指针
     */
    void addHeader(const char *start, const char *colon, const char *end);

    /**
     * @brief 获取指定头部
     * @param key 头部名
     * @return 可选值,存在则为头部值
     */
    std::optional<std::string> getHeader(const std::string &key) const;

    /**
     * @brief 获取所有头部
     * @return 头部map的常引用
     */
    const std::unordered_map<std::string, std::string> &headers() const { return headers_; }

    /**
     * @brief 设置消息体
     * @param start 体起始指针
     * @param len   体长度
     */
    void setBody(const char *start, size_t len);

    /**
     * @brief 获取消息体
     * @return 消息体字符串
     */
    const std::string &body() const { return body_; }
    std::string &body() { return body_; }

    void swap(HttpRequest &that);

private:
    Method method_;                                        // 请求方法
    Version version_;                                      // 协议版本
    std::string path_;                                     // 请求路径
    std::string query_;                                    // 查询参数
    std::unordered_map<std::string, std::string> headers_; // 头部字段
    std::string body_;                                     // 消息体
};