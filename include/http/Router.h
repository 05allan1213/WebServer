#pragma once

#include "base/noncopyable.h"
#include "net/Callbacks.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketHandler.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <memory>
#include <stdexcept>

/**
 * @brief 路由节点，用于构建路由树
 *
 * 存储特定路径的处理函数链和参数名列表。
 */
struct RouteNode
{
    std::unordered_map<std::string, MiddlewareChain> handlers; // key: HTTP方法
    std::vector<std::string> paramNames;                       // 按顺序存储参数名，例如：/users/:id存储{"id"}
};

/**
 * @brief 路由匹配结果
 *
 * 包含匹配状态、处理函数链和提取的路径参数。
 */
struct RouteMatchResult
{
    bool matched = false;                                // 是否匹配成功
    MiddlewareChain chain;                               // 包含组合后链的副本
    std::unordered_map<std::string, std::string> params; // 提取出的路径参数
};

/**
 * @brief HTTP路由器，负责管理路由和中间件
 *
 * 支持精确匹配和正则匹配两种路由方式。
 * 支持中间件链和WebSocket路由。
 */
class Router : private noncopyable
{
public:
    /**
     * @brief 构造函数
     */
    Router();

    /**
     * @brief 添加全局中间件
     * @param middleware 中间件函数
     */
    void use(Middleware middleware);

    /**
     * @brief 添加路由（模板方法）
     * @param method HTTP方法
     * @param path 路径模式
     * @param handlers 处理函数列表
     */
    template <typename... Handlers>
    void add(const std::string &method, const std::string &path, Handlers... handlers);

    /**
     * @brief 添加WebSocket路由
     * @param path 路径
     * @param handler WebSocket处理器
     */
    void addWebSocket(const std::string &path, WebSocketHandler::Ptr handler);

    /**
     * @brief 添加所有方法的路由
     * @param path 路径
     * @param handlers 处理函数列表
     */
    template <typename... Handlers>
    void all(const std::string &path, Handlers... handlers)
    {
        add("*", path, handlers...);
    }

    /**
     * @brief 添加GET路由
     * @param path 路径
     * @param handlers 处理函数列表
     */
    template <typename... Handlers>
    void get(const std::string &path, Handlers... handlers)
    {
        add("GET", path, handlers...);
    }

    /**
     * @brief 添加POST路由
     * @param path 路径
     * @param handlers 处理函数列表
     */
    template <typename... Handlers>
    void post(const std::string &path, Handlers... handlers)
    {
        add("POST", path, handlers...);
    }

    /**
     * @brief 匹配HTTP路由
     * @param method HTTP方法
     * @param path 请求路径
     * @return 路由匹配结果
     */
    RouteMatchResult match(const std::string &method, const std::string &path) const;

    /**
     * @brief 匹配WebSocket路由
     * @param req HTTP请求对象
     * @return WebSocket处理器指针
     */
    WebSocketHandler::Ptr matchWebSocket(const HttpRequest &req) const;

private:
    /**
     * @brief 包装HTTP处理函数为中间件
     * @param handler HTTP处理函数
     * @return 中间件函数
     */
    Middleware wrapHandler(HttpHandler handler);

    /**
     * @brief 构建中间件链（模板方法）
     * @param chain 中间件链
     * @param first 第一个处理函数
     * @param rest 剩余处理函数
     */
    template <typename T, typename... Rest>
    void buildChain(MiddlewareChain &chain, T first, Rest... rest);

    /**
     * @brief 构建中间件链（递归终止）
     * @param chain 中间件链
     */
    void buildChain(MiddlewareChain &chain) {} // 递归终止

    MiddlewareChain globalMiddlewares_;                                  // 全局中间件链
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> routes_; // 精确匹配路由表
    std::vector<std::pair<std::regex, RouteNode *>> regexRoutes_;        // 正则匹配路由表
    std::unordered_map<std::string, WebSocketHandler::Ptr> wsRoutes_;    // WebSocket路由表
};

// --- 模板实现必须在头文件中 ---

template <typename... Handlers>
void Router::add(const std::string &method, const std::string &path, Handlers... handlers)
{
    MiddlewareChain chain;
    buildChain(chain, handlers...);

    std::unique_ptr<RouteNode> node = std::make_unique<RouteNode>();
    node->handlers[method] = std::move(chain);

    // 检查路径是否包含参数（:param）或通配符（*）
    if (path.find(':') != std::string::npos || path.find('*') != std::string::npos)
    {
        std::string regexPath = path;
        std::regex paramRegex(":([a-zA-Z0-9_]+)");
        auto words_begin = std::sregex_iterator(path.begin(), path.end(), paramRegex);
        auto words_end = std::sregex_iterator();

        // 提取参数名并存储
        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
            node->paramNames.push_back((*i)[1].str());
        }

        // 将路径模式转换为正则表达式
        regexPath = std::regex_replace(regexPath, paramRegex, "([^/]+)");
        regexPath = std::regex_replace(regexPath, std::regex("\\*"), ".*");

        try
        {
            // 将节点的所有权移到routes_ map中，并让regexRoutes_持有裸指针
            RouteNode *rawNodePtr = node.get();
            routes_[path] = std::move(node);
            regexRoutes_.emplace_back(std::regex("^" + regexPath + "$"), rawNodePtr);
        }
        catch (const std::regex_error &e)
        {
            throw std::runtime_error("Invalid regex in path: " + path);
        }
    }
    else // 精确匹配路径
    {
        routes_[path] = std::move(node);
    }
}

template <typename T, typename... Rest>
void Router::buildChain(MiddlewareChain &chain, T first, Rest... rest)
{
    if constexpr (std::is_convertible_v<T, Middleware>)
    {
        chain.push_back(first);
    }
    else
    {
        chain.push_back(wrapHandler(first));
    }
    buildChain(chain, rest...);
}