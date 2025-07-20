#include "http/HttpServer.h"
#include "net/NetworkConfig.h"

int main()
{
    NetworkConfig::getInstance().load("configs/config.yml");
    const auto &netConfig = NetworkConfig::getInstance();
    std::string ip = netConfig.getIp();
    int port = netConfig.getPort();
    int threadNum = netConfig.getThreadNum();

    EventLoop loop;
    InetAddress addr(port, ip);
    HttpServer server(&loop, addr, "HttpServer-01", threadNum);

    server.setHttpCallback([](const HttpRequest &req, HttpResponse *resp)
                           {
        if (req.path() == "/") {
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/html");
            resp->setBody("<html><body><h1>Welcome</h1><p>This is your C++ WebServer.</p></body></html>");
        } else {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        } });

    server.start();
    loop.loop();
    return 0;
}
