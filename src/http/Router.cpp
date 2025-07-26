#include "http/Router.h"
#include "log/Log.h"

Router::Router() {}

void Router::use(Middleware middleware)
{
    globalMiddlewares_.push_back(std::move(middleware));
}

void Router::addWebSocket(const std::string &path, WebSocketHandler::Ptr handler)
{
    DLOG_INFO << "[Router] 添加WebSocket路由: " << path;
    wsRoutes_[path] = handler;
}

WebSocketHandler::Ptr Router::matchWebSocket(const HttpRequest &req) const
{
    auto it = wsRoutes_.find(req.getPath());
    if (it != wsRoutes_.end())
    {
        return it->second;
    }
    return nullptr;
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
                // 提取路径参数
                if (match.size() > 1)
                {
                    for (size_t i = 1; i < match.size(); ++i)
                    {
                        if (i - 1 < node->paramNames.size())
                        {
                            result.params[node->paramNames[i - 1]] = match[i].str();
                        }
                    }
                }
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