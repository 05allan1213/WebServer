#include "http/Router.h"
#include "log/Log.h"

/**
 * @brief 构造函数
 */
Router::Router() {}

/**
 * @brief 添加全局中间件
 * @param middleware 中间件函数
 */
void Router::use(Middleware middleware)
{
    globalMiddlewares_.push_back(std::move(middleware));
}

/**
 * @brief 添加WebSocket路由
 * @param path 路径
 * @param handler WebSocket处理器
 */
void Router::addWebSocket(const std::string &path, WebSocketHandler::Ptr handler)
{
    DLOG_INFO << "[Router] 添加WebSocket路由: " << path;
    wsRoutes_[path] = handler;
}

/**
 * @brief 匹配WebSocket路由
 * @param req HTTP请求对象
 * @return WebSocket处理器指针，未匹配返回nullptr
 */
WebSocketHandler::Ptr Router::matchWebSocket(const HttpRequest &req) const
{
    auto it = wsRoutes_.find(req.getPath());
    if (it != wsRoutes_.end())
    {
        return it->second;
    }
    return nullptr;
}

/**
 * @brief 包装HTTP处理函数为中间件
 * @param handler HTTP处理函数
 * @return 中间件函数
 */
Middleware Router::wrapHandler(HttpHandler handler)
{
    return [handler](const HttpRequest &req, HttpResponse *resp, Next)
    {
        handler(req, resp);
    };
}

/**
 * @brief 匹配HTTP路由
 * @param method HTTP方法
 * @param path 请求路径
 * @return 路由匹配结果
 *
 * 匹配策略：
 * 1. 优先进行精确匹配
 * 2. 如果精确匹配失败，尝试正则表达式匹配
 * 3. 支持通配符方法（*）
 * 4. 自动提取路径参数
 */
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
                node = pair.second.get();
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
        // 查找对应的HTTP方法处理器
        auto handler_it = node->handlers.find(method);
        if (handler_it == node->handlers.end())
        {
            // 如果没找到，尝试通配符方法（*）
            handler_it = node->handlers.find("*");
        }

        if (handler_it != node->handlers.end())
        {
            result.matched = true;
            // 组合全局中间件和路由中间件
            result.chain = globalMiddlewares_;
            result.chain.insert(result.chain.end(), handler_it->second.begin(), handler_it->second.end());
            return result;
        }
    }

    return result;
}