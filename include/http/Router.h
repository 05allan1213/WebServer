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
 */
struct RouteNode
{
    std::unordered_map<std::string, MiddlewareChain> handlers; // key: HTTP Method
    std::vector<std::string> paramNames;                       // 按顺序存储参数名, e.g., for /users/:id, this stores {"id"}
};

/**
 * @brief 路由匹配结果
 */
struct RouteMatchResult
{
    bool matched = false;
    MiddlewareChain chain;                               // 包含组合后链的副本
    std::unordered_map<std::string, std::string> params; // 提取出的路径参数
};

/**
 * @brief HTTP 路由器，负责管理路由和中间件
 */
class Router : private noncopyable
{
public:
    Router();

    void use(Middleware middleware);

    template <typename... Handlers>
    void add(const std::string &method, const std::string &path, Handlers... handlers);

    void addWebSocket(const std::string &path, WebSocketHandler::Ptr handler);

    template <typename... Handlers>
    void all(const std::string &path, Handlers... handlers)
    {
        add("*", path, handlers...);
    }

    template <typename... Handlers>
    void get(const std::string &path, Handlers... handlers)
    {
        add("GET", path, handlers...);
    }

    template <typename... Handlers>
    void post(const std::string &path, Handlers... handlers)
    {
        add("POST", path, handlers...);
    }

    RouteMatchResult match(const std::string &method, const std::string &path) const;
    WebSocketHandler::Ptr matchWebSocket(const HttpRequest &req) const;

private:
    Middleware wrapHandler(HttpHandler handler);

    template <typename T, typename... Rest>
    void buildChain(MiddlewareChain &chain, T first, Rest... rest);
    void buildChain(MiddlewareChain &chain) {} // 递归终止

    MiddlewareChain globalMiddlewares_;
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> routes_;
    // 将正则表达式路由的结构体修改为包含 RouteNode 指针
    std::vector<std::pair<std::regex, RouteNode *>> regexRoutes_;
    std::unordered_map<std::string, WebSocketHandler::Ptr> wsRoutes_;
};

// --- Template Implementations Must Be in Header ---

template <typename... Handlers>
void Router::add(const std::string &method, const std::string &path, Handlers... handlers)
{
    MiddlewareChain chain;
    buildChain(chain, handlers...);

    std::unique_ptr<RouteNode> node = std::make_unique<RouteNode>();
    node->handlers[method] = std::move(chain);

    // 检查路径是否包含参数
    if (path.find(':') != std::string::npos || path.find('*') != std::string::npos)
    {
        std::string regexPath = path;
        std::regex paramRegex(":([a-zA-Z0-9_]+)");
        auto words_begin = std::sregex_iterator(path.begin(), path.end(), paramRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
            node->paramNames.push_back((*i)[1].str());
        }

        regexPath = std::regex_replace(regexPath, paramRegex, "([^/]+)");
        regexPath = std::regex_replace(regexPath, std::regex("\\*"), ".*");

        try
        {
            // 将节点的所有权移到 routes_ map 中，并让 regexRoutes_ 持有裸指针
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