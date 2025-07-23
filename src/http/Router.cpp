#include "http/Router.h"
#include "log/Log.h"
#include <regex>

Router::Router() {}

void Router::use(Middleware middleware)
{
    globalMiddlewares_.push_back(std::move(middleware));
}

Middleware Router::wrapHandler(HttpHandler handler)
{
    return [handler](const HttpRequest &req, HttpResponse *resp, Next)
    {
        handler(req, resp);
    };
}

RouteMatchResult Router::match(const std::string &method, const std::string &path) const
{
    RouteMatchResult result;
    const RouteNode *node = nullptr;

    // 1. 优先进行精确匹配
    auto it = routes_.find(path);
    if (it != routes_.end())
    {
        node = it->second.get();
    }
    else
    {
        // 2. 如果精确匹配失败，尝试正则表达式匹配
        for (const auto &pair : regexRoutes_)
        {
            std::smatch match;
            if (std::regex_match(path, match, pair.first))
            {
                node = pair.second;
                // TODO: 参数提取逻辑
                break;
            }
        }
    }

    if (node)
    {
        auto handler_it = node->handlers.find(method);
        if (handler_it == node->handlers.end())
        {
            handler_it = node->handlers.find("*"); // 尝试通配方法
        }

        if (handler_it != node->handlers.end())
        {
            result.matched = true;
            result.chain = globalMiddlewares_;
            result.chain.insert(result.chain.end(), handler_it->second.begin(), handler_it->second.end());
            return result;
        }
    }

    return result;
}