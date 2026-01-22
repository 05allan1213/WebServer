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

// 回归测试：同一路径多 method
TEST_F(RouterTest, SamePathMultipleMethods)
{
    bool get_called = false;
    bool post_called = false;
    bool put_called = false;

    router->get("/api/users", [&](const HttpRequest &, HttpResponse *) { get_called = true; });
    router->post("/api/users", [&](const HttpRequest &, HttpResponse *) { post_called = true; });
    router->add("PUT", "/api/users", [&](const HttpRequest &, HttpResponse *) { put_called = true; });

    auto get_result = router->match("GET", "/api/users");
    ASSERT_TRUE(get_result.matched);
    HttpResponse resp1(false);
    auto next1 = []() {};
    if (!get_result.chain.empty())
    {
        get_result.chain.front()(HttpRequest(), &resp1, next1);
    }
    EXPECT_TRUE(get_called);
    EXPECT_FALSE(post_called);
    EXPECT_FALSE(put_called);

    auto post_result = router->match("POST", "/api/users");
    ASSERT_TRUE(post_result.matched);
    HttpResponse resp2(false);
    auto next2 = []() {};
    if (!post_result.chain.empty())
    {
        post_result.chain.front()(HttpRequest(), &resp2, next2);
    }
    EXPECT_TRUE(post_called);
    EXPECT_FALSE(put_called);

    auto put_result = router->match("PUT", "/api/users");
    ASSERT_TRUE(put_result.matched);
    HttpResponse resp3(false);
    auto next3 = []() {};
    if (!put_result.chain.empty())
    {
        put_result.chain.front()(HttpRequest(), &resp3, next3);
    }
    EXPECT_TRUE(put_called);
}

// 回归测试：参数路由提参
TEST_F(RouterTest, ParametricRouteExtraction)
{
    std::string userId, postId;
    router->get("/users/:userId/posts/:postId", [&](const HttpRequest &req, HttpResponse *)
                {
        userId = req.getParam("userId").value_or("");
        postId = req.getParam("postId").value_or(""); });

    auto result = router->match("GET", "/users/alice/posts/42");
    ASSERT_TRUE(result.matched);
    EXPECT_EQ(result.params["userId"], "alice");
    EXPECT_EQ(result.params["postId"], "42");

    HttpRequest req;
    req.setParams(result.params);
    HttpResponse resp(false);
    auto next = []() {};
    if (!result.chain.empty())
    {
        result.chain.front()(req, &resp, next);
    }

    EXPECT_EQ(userId, "alice");
    EXPECT_EQ(postId, "42");
}

// 回归测试：/* 通配路由顺序截胡
TEST_F(RouterTest, WildcardRouteOrder)
{
    bool wildcard_called = false;
    bool specific_called = false;

    // 先注册通配路由
    router->get("/*", [&](const HttpRequest &, HttpResponse *) { wildcard_called = true; });
    // 后注册精确路由
    router->get("/specific", [&](const HttpRequest &, HttpResponse *) { specific_called = true; });

    // 精确匹配优先
    auto result1 = router->match("GET", "/specific");
    ASSERT_TRUE(result1.matched);
    HttpResponse resp1(false);
    auto next1 = []() {};
    if (!result1.chain.empty())
    {
        result1.chain.front()(HttpRequest(), &resp1, next1);
    }
    EXPECT_TRUE(specific_called);
    EXPECT_FALSE(wildcard_called);

    // 通配匹配
    wildcard_called = false;
    specific_called = false;
    auto result2 = router->match("GET", "/anything/else");
    ASSERT_TRUE(result2.matched);
    HttpResponse resp2(false);
    auto next2 = []() {};
    if (!result2.chain.empty())
    {
        result2.chain.front()(HttpRequest(), &resp2, next2);
    }
    EXPECT_TRUE(wildcard_called);
    EXPECT_FALSE(specific_called);
}

// 回归测试：正则路由注册顺序影响匹配
TEST_F(RouterTest, RegexRouteRegistrationOrder)
{
    bool first_called = false;
    bool second_called = false;

    // 先注册的正则路由优先匹配
    router->get("/api/*", [&](const HttpRequest &, HttpResponse *) { first_called = true; });
    router->get("/api/users/:id", [&](const HttpRequest &, HttpResponse *) { second_called = true; });

    auto result = router->match("GET", "/api/users/123");
    ASSERT_TRUE(result.matched);
    HttpResponse resp(false);
    auto next = []() {};
    if (!result.chain.empty())
    {
        result.chain.front()(HttpRequest(), &resp, next);
    }
    // 因为 /api/* 先注册，所以会被它截胡
    EXPECT_TRUE(first_called);
    EXPECT_FALSE(second_called);
}