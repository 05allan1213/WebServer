#pragma once

#include "base/noncopyable.h"
#include "net/Callbacks.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
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
};

/**
 * @brief 路由匹配结果
 */
struct RouteMatchResult
{
    bool matched = false;
    MiddlewareChain chain; // 包含组合后链的副本
    std::unordered_map<std::string, std::string> params;
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

private:
    Middleware wrapHandler(HttpHandler handler);

    template <typename T, typename... Rest>
    void buildChain(MiddlewareChain &chain, T first, Rest... rest);
    void buildChain(MiddlewareChain &chain) {} // 递归终止

    MiddlewareChain globalMiddlewares_;
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> routes_;
    std::vector<std::pair<std::regex, RouteNode *>> regexRoutes_;
};

// --- Template Implementations Must Be in Header ---

template <typename... Handlers>
void Router::add(const std::string &method, const std::string &path, Handlers... handlers)
{
    MiddlewareChain chain;
    buildChain(chain, handlers...);

    if (routes_.find(path) == routes_.end())
    {
        routes_[path] = std::make_unique<RouteNode>();
    }
    routes_[path]->handlers[method] = std::move(chain);

    if (path.find(':') != std::string::npos || path.find('*') != std::string::npos)
    {
        try
        {
            std::string regexPath = path;
            regexPath = std::regex_replace(regexPath, std::regex(":([a-zA-Z0-9_]+)"), "([a-zA-Z0-9_]+)");
            regexPath = std::regex_replace(regexPath, std::regex("\\*"), ".*");
            regexRoutes_.emplace_back(std::regex("^" + regexPath + "$"), routes_[path].get());
        }
        catch (const std::regex_error &e)
        {
            throw std::runtime_error("Invalid regex in path: " + path);
        }
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