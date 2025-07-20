#pragma once
#include "HttpRequest.h"
#include <string>

class Buffer; // 前向声明

/**
 * @brief HTTP请求解析器,负责将原始数据流解析为HttpRequest对象
 *
 * 用法：
 * 1. 每个连接分配一个HttpParser实例,持续复用
 * 2. 调用parseRequest驱动解析,支持分段数据
 * 3. 解析完成后可通过request()获取请求对象
 */
class HttpParser
{
public:
    /**
     * @brief HTTP请求解析状态
     */
    enum class HttpRequestParseState
    {
        kExpectRequestLine, // 期待请求行
        kExpectHeaders,     // 期待头部
        kExpectBody,        // 期待消息体
        kGotAll             // 解析完成
    };

    /**
     * @brief 构造函数,初始化解析状态
     */
    HttpParser();

    /**
     * @brief 重置解析器状态,准备解析下一个请求
     */
    void reset();

    /**
     * @brief 解析HTTP请求
     * @param buf 输入缓冲区
     * @return 解析是否成功
     *
     * 支持分段数据,自动维护状态机。
     */
    bool parseRequest(Buffer *buf);

    /**
     * @brief 判断是否解析完成
     * @return true表示已完成一个完整请求
     */
    bool gotAll() const { return state_ == HttpRequestParseState::kGotAll; }

    /**
     * @brief 获取解析得到的HttpRequest对象
     * @return const引用,包含请求所有信息
     */
    const HttpRequest &request() const { return request_; }

private:
    /**
     * @brief 解析请求行
     * @param begin 起始指针
     * @param end   结束指针
     * @return 是否解析成功
     */
    bool parseRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_; // 当前解析状态
    HttpRequest request_;         // 当前解析得到的请求对象
};