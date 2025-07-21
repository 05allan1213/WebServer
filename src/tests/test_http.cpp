#include "http/HttpServer.h"
#include "net/NetworkConfig.h"
#include "http/StaticFileHandler.h"
#include "log/Log.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

// 辅助函数：将Method枚举转为字符串
std::string methodToString(HttpRequest::Method method)
{
    switch (method)
    {
    case HttpRequest::Method::kGet:
        return "GET";
    case HttpRequest::Method::kPost:
        return "POST";
    case HttpRequest::Method::kHead:
        return "HEAD";
    case HttpRequest::Method::kPut:
        return "PUT";
    case HttpRequest::Method::kDelete:
        return "DELETE";
    case HttpRequest::Method::kInvalid:
        return "INVALID";
    default:
        return "UNKNOWN";
    }
}

// 读取文件内容
std::string readFile(const std::string &path)
{
    std::ifstream ifs(path);
    if (!ifs)
        return "";
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

// 列出目录中的所有文件
void listDirectoryFiles(const std::string &path, const std::string &indent = "")
{
    try
    {
        for (const auto &entry : std::filesystem::directory_iterator(path))
        {
            DLOG_INFO << indent << entry.path().filename().string();
            if (entry.is_directory())
            {
                listDirectoryFiles(entry.path().string(), indent + "  ");
            }
        }
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "列出目录失败: " << e.what();
    }
}

// 测试访问各种HTTP状态码页面
void testErrorPages()
{
    DLOG_INFO << "测试错误页面...";
    std::vector<std::string> errorPages = {
        "/404.html",
        "/400.html",
        "/403.html",
        "/500.html"};
    DLOG_INFO << "可以通过以下URL测试错误页面:";
    for (const auto &page : errorPages)
    {
        DLOG_INFO << "http://localhost:8080" << page;
    }
}

int main()
{
    // 加载网络配置
    NetworkConfig::getInstance().load("configs/config.yml");
    const auto &netConfig = NetworkConfig::getInstance();
    std::string ip = netConfig.getIp();
    int port = netConfig.getPort();
    int threadNum = netConfig.getThreadNum();

    DLOG_INFO << "启动HTTP测试服务器...";
    DLOG_INFO << "服务器配置: IP=" << ip << ", 端口=" << port << ", 线程数=" << threadNum;

    // 列出静态文件目录中的所有文件
    DLOG_INFO << "静态资源目录内容:";
    listDirectoryFiles("web_static");

    EventLoop loop;
    InetAddress addr(port, ip);
    HttpServer server(&loop, addr, "HttpServer-01", threadNum);

    server.setHttpCallback([](const HttpRequest &req, HttpResponse *resp)
                           {
        DLOG_INFO << "收到请求: " << methodToString(req.getMethod()) << " " << req.getPath();
        // 尝试处理静态文件
        if (StaticFileHandler::handle(req, resp)) {
            DLOG_INFO << "成功处理静态文件: " << req.getPath();
            return;
        }
        // 如果不是静态文件，返回404
        DLOG_WARN << "未找到资源: " << req.getPath();
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/html");
        std::string notFoundPage = readFile("web_static/404.html");
        if (!notFoundPage.empty()) {
            resp->setBody(notFoundPage);
        } else {
            resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        } });

    DLOG_INFO << "HTTP测试服务器启动成功，监听地址: http://" << ip << ":" << port;
    DLOG_INFO << "可通过浏览器访问以下URL进行测试:";
    DLOG_INFO << "首页: http://" << ip << ":" << port << "/index.html";
    DLOG_INFO << "关于页: http://" << ip << ":" << port << "/about.html";
    DLOG_INFO << "联系页: http://" << ip << ":" << port << "/contact.html";
    testErrorPages();

    server.start();
    loop.loop();
    return 0;
}
