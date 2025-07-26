#include <gtest/gtest.h>
#include "http/Router.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketHandler.h"

class MockWebSocketHandler : public WebSocketHandler
{
public:
    void onConnect(const TcpConnectionPtr &conn) override {}
    void onMessage(const TcpConnectionPtr &conn, const std::string &message) override {}
    void onClose(const TcpConnectionPtr &conn) override {}
};

class RouterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        router = std::make_unique<Router>();
    }

    std::unique_ptr<Router> router;
};

TEST_F(RouterTest, HandlesExactMatch)
{
    bool handler_called = false;
    router->get("/home", [&](const HttpRequest &, HttpResponse *)
                { handler_called = true; });

    auto result = router->match("GET", "/home");
    ASSERT_TRUE(result.matched);

    HttpResponse resp(false);
    auto next = [&]() {};
    if (!result.chain.empty())
    {
        result.chain.front()(HttpRequest(), &resp, next);
    }

    EXPECT_TRUE(handler_called);
}

TEST_F(RouterTest, HandlesParametricMatch)
{
    std::string userId;
    router->get("/users/:id", [&](const HttpRequest &req, HttpResponse *)
                { userId = req.getParam("id").value_or(""); });

    auto result = router->match("GET", "/users/123");
    ASSERT_TRUE(result.matched);
    EXPECT_EQ(result.params["id"], "123");

    HttpRequest req;
    req.setParams(result.params);
    HttpResponse resp(false);
    auto next = [&]() {};
    if (!result.chain.empty())
    {
        result.chain.front()(req, &resp, next);
    }

    EXPECT_EQ(userId, "123");
}

TEST_F(RouterTest, HandlesWebSocketMatch)
{
    auto handler = std::make_shared<MockWebSocketHandler>();
    router->addWebSocket("/ws", handler);

    HttpRequest req;
    std::string path = "/ws";
    req.setPath(path.c_str(), path.c_str() + path.length());

    auto matched_handler = router->matchWebSocket(req);
    ASSERT_NE(matched_handler, nullptr);
    EXPECT_EQ(matched_handler, handler);
}

TEST_F(RouterTest, HandlesNotFound)
{
    router->get("/about", [&](const HttpRequest &, HttpResponse *) {});
    auto result = router->match("GET", "/non-existent-page");
    EXPECT_FALSE(result.matched);
}