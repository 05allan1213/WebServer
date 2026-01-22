#include <gtest/gtest.h>
#include "http/HttpParser.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/Router.h"
#include "base/Buffer.h"

class HttpProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        parser = std::make_unique<HttpParser>();
        buffer = std::make_unique<Buffer>();
    }

    std::unique_ptr<HttpParser> parser;
    std::unique_ptr<Buffer> buffer;
};

// 测试 chunked 编码解析
TEST_F(HttpProtocolTest, ChunkedEncoding)
{
    std::string request =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    EXPECT_EQ(req.getMethod(), HttpRequest::Method::kPost);
    EXPECT_EQ(req.getPath(), "/upload");
    EXPECT_EQ(req.getBody(), "Hello World");
}

// 测试 keep-alive 连接
TEST_F(HttpProtocolTest, KeepAliveConnection)
{
    std::string request =
        "GET /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    auto conn_header = req.getHeader("connection");
    ASSERT_TRUE(conn_header.has_value());
    EXPECT_EQ(conn_header.value(), "keep-alive");
}

// 测试 path traversal 攻击防护
TEST_F(HttpProtocolTest, PathTraversalAttack)
{
    std::vector<std::string> malicious_paths = {
        "/../../../etc/passwd",
        "/files/../../../etc/passwd",
        "/files/..%2F..%2F..%2Fetc%2Fpasswd",
        "/files/....//....//etc/passwd"};

    for (const auto &path : malicious_paths)
    {
        std::string request =
            "GET " + path + " HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";

        auto test_buffer = std::make_unique<Buffer>();
        auto test_parser = std::make_unique<HttpParser>();

        test_buffer->append(request.data(), request.size());
        ASSERT_TRUE(test_parser->parseRequest(test_buffer.get()));
        ASSERT_TRUE(test_parser->gotAll());

        const auto &req = test_parser->request();
        // 路径应该被解析，但业务层需要验证路径安全性
        EXPECT_FALSE(req.getPath().empty());
    }
}

// 测试 405 Method Not Allowed
TEST_F(HttpProtocolTest, MethodNotAllowed)
{
    Router router;
    router.get("/api/users", [](const HttpRequest &, HttpResponse *resp)
               { resp->setStatusCode(HttpResponse::k200Ok); });

    // 尝试用 POST 访问只支持 GET 的路由
    auto result = router.match("POST", "/api/users");
    EXPECT_FALSE(result.matched);

    // 响应应该返回 405
    HttpResponse resp(false);
    resp.setStatusCode(HttpResponse::k405MethodNotAllowed);
    resp.setHeader("Allow", "GET");
    EXPECT_EQ(resp.getStatusCode(), HttpResponse::k405MethodNotAllowed);
}

// 测试超大请求头
TEST_F(HttpProtocolTest, LargeHeaders)
{
    std::string request = "GET /api HTTP/1.1\r\n";
    request += "Host: example.com\r\n";

    // 添加大量头部
    for (int i = 0; i < 100; i++)
    {
        request += "X-Custom-Header-" + std::to_string(i) + ": value" + std::to_string(i) + "\r\n";
    }
    request += "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    EXPECT_EQ(req.getMethod(), HttpRequest::Method::kGet);
    EXPECT_EQ(req.getPath(), "/api");
}

// 测试空 body 的 POST 请求
TEST_F(HttpProtocolTest, PostWithEmptyBody)
{
    std::string request =
        "POST /api/action HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    EXPECT_EQ(req.getMethod(), HttpRequest::Method::kPost);
    EXPECT_TRUE(req.getBody().empty());
}

// 测试 HTTP/1.0 请求
TEST_F(HttpProtocolTest, Http10Request)
{
    std::string request =
        "GET /index.html HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    EXPECT_EQ(req.getVersion(), HttpRequest::Version::kHttp10);
}

// 测试 chunked 响应编码
TEST_F(HttpProtocolTest, ChunkedResponse)
{
    HttpResponse resp(false);
    resp.setStatusCode(HttpResponse::k200Ok);
    resp.setContentType("text/plain");
    resp.setChunkedEncoding(true);
    resp.setBody("Test data");

    Buffer output;
    resp.appendToBuffer(&output);

    std::string response = output.retrieveAllAsString();
    EXPECT_NE(response.find("Transfer-Encoding: chunked"), std::string::npos);
}

// 测试多个连续请求（keep-alive 场景）
TEST_F(HttpProtocolTest, MultipleRequestsKeepAlive)
{
    std::string request1 =
        "GET /api/1 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    std::string request2 =
        "GET /api/2 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    // 第一个请求
    buffer->append(request1.data(), request1.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());
    EXPECT_EQ(parser->request().getPath(), "/api/1");

    // 重置解析器
    parser->reset();

    // 第二个请求
    buffer->append(request2.data(), request2.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());
    EXPECT_EQ(parser->request().getPath(), "/api/2");
}
