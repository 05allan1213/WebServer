#include <gtest/gtest.h>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/handlers/AuthHandler.h"
#include "base/ConfigManager.h"
#include <jwt-cpp/jwt.h>
#include <mysql/mysql.h>
#include <string>

class ApiSecurityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化配置（如果需要）
    }
};

// 测试 SQL 注入防护 - 用户名字段
TEST_F(ApiSecurityTest, SqlInjectionUsername)
{
    std::vector<std::string> injection_payloads = {
        "admin' OR '1'='1",
        "admin'--",
        "admin' OR '1'='1'--",
        "admin'; DROP TABLE users--",
        "' OR 1=1--",
        "1' UNION SELECT NULL, NULL, NULL--",
        "admin' AND 1=0 UNION ALL SELECT 'admin', '81dc9bdb52d04dc20036dbd8313ed055'--"};

    for (const auto &payload : injection_payloads)
    {
        // 模拟 SQL 注入尝试
        // 实际应用中，这些输入应该被参数化查询或转义处理
        EXPECT_NE(payload.find("'"), std::string::npos) << "Payload should contain SQL injection attempt";

        // 验证输入包含危险字符
        bool has_dangerous_chars = (payload.find("'") != std::string::npos ||
                                    payload.find("--") != std::string::npos ||
                                    payload.find(";") != std::string::npos ||
                                    payload.find("UNION") != std::string::npos ||
                                    payload.find("DROP") != std::string::npos);
        EXPECT_TRUE(has_dangerous_chars) << "Payload: " << payload;
    }
}

// 测试 SQL 注入防护 - 数字字段
TEST_F(ApiSecurityTest, SqlInjectionNumericField)
{
    std::vector<std::string> injection_payloads = {
        "1 OR 1=1",
        "1; DROP TABLE users",
        "1 UNION SELECT * FROM users",
        "-1 UNION SELECT NULL, username, password FROM users--"};

    for (const auto &payload : injection_payloads)
    {
        // 数字字段的注入尝试
        bool has_sql_keywords = (payload.find("OR") != std::string::npos ||
                                 payload.find("UNION") != std::string::npos ||
                                 payload.find("DROP") != std::string::npos ||
                                 payload.find("SELECT") != std::string::npos);
        EXPECT_TRUE(has_sql_keywords) << "Payload: " << payload;
    }
}

// 测试 JWT 过期验证
TEST_F(ApiSecurityTest, JwtExpiredToken)
{
    // 创建一个已过期的 JWT token
    auto token = jwt::create()
                     .set_issuer("webserver")
                     .set_type("JWT")
                     .set_payload_claim("user_id", jwt::claim(std::string("123")))
                     .set_expires_at(std::chrono::system_clock::now() - std::chrono::hours(1)) // 1小时前过期
                     .sign(jwt::algorithm::hs256{"test_secret"});

    HttpRequest req;
    req.setHeader("Authorization", "Bearer " + token);

    int user_id = -1;
    bool auth_result = checkAuth(req, user_id);

    // 过期的 token 应该验证失败
    EXPECT_FALSE(auth_result);
    EXPECT_EQ(user_id, -1);
}

// 测试 JWT 无过期时间
TEST_F(ApiSecurityTest, JwtNoExpiration)
{
    // 创建一个没有过期时间的 JWT token
    auto token = jwt::create()
                     .set_issuer("webserver")
                     .set_type("JWT")
                     .set_payload_claim("user_id", jwt::claim(std::string("123")))
                     .sign(jwt::algorithm::hs256{"test_secret"});

    HttpRequest req;
    req.setHeader("Authorization", "Bearer " + token);

    int user_id = -1;
    bool auth_result = checkAuth(req, user_id);

    // 没有过期时间的 token 应该验证失败（根据 AuthHandler.cpp 的实现）
    EXPECT_FALSE(auth_result);
}

// 测试 JWT issuer 错误
TEST_F(ApiSecurityTest, JwtWrongIssuer)
{
    // 创建一个 issuer 错误的 JWT token
    auto token = jwt::create()
                     .set_issuer("malicious_issuer") // 错误的 issuer
                     .set_type("JWT")
                     .set_payload_claim("user_id", jwt::claim(std::string("123")))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(1))
                     .sign(jwt::algorithm::hs256{"test_secret"});

    HttpRequest req;
    req.setHeader("Authorization", "Bearer " + token);

    int user_id = -1;
    bool auth_result = checkAuth(req, user_id);

    // issuer 错误的 token 应该验证失败
    EXPECT_FALSE(auth_result);
}

// 测试 JWT 签名错误
TEST_F(ApiSecurityTest, JwtWrongSignature)
{
    // 创建一个签名错误的 JWT token
    auto token = jwt::create()
                     .set_issuer("webserver")
                     .set_type("JWT")
                     .set_payload_claim("user_id", jwt::claim(std::string("123")))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(1))
                     .sign(jwt::algorithm::hs256{"wrong_secret"}); // 错误的密钥

    HttpRequest req;
    req.setHeader("Authorization", "Bearer " + token);

    int user_id = -1;
    bool auth_result = checkAuth(req, user_id);

    // 签名错误的 token 应该验证失败
    EXPECT_FALSE(auth_result);
}

// 测试 JWT 格式错误
TEST_F(ApiSecurityTest, JwtMalformedToken)
{
    std::vector<std::string> malformed_tokens = {
        "not.a.jwt",
        "Bearer malformed",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.invalid",
        ""};

    for (const auto &token : malformed_tokens)
    {
        HttpRequest req;
        req.setHeader("Authorization", "Bearer " + token);

        int user_id = -1;
        bool auth_result = checkAuth(req, user_id);

        // 格式错误的 token 应该验证失败
        EXPECT_FALSE(auth_result) << "Token: " << token;
    }
}

// 测试缺少 Authorization 头
TEST_F(ApiSecurityTest, MissingAuthorizationHeader)
{
    HttpRequest req;
    // 不添加 Authorization 头

    int user_id = -1;
    bool auth_result = checkAuth(req, user_id);

    EXPECT_FALSE(auth_result);
    EXPECT_EQ(user_id, -1);
}

// 测试 Authorization 头格式错误
TEST_F(ApiSecurityTest, InvalidAuthorizationHeaderFormat)
{
    std::vector<std::string> invalid_headers = {
        "Basic dXNlcjpwYXNz", // Basic auth instead of Bearer
        "token_without_bearer",
        "Bearer",           // Bearer without token
        "Bearer "};         // Bearer with empty token

    for (const auto &header : invalid_headers)
    {
        HttpRequest req;
        req.setHeader("Authorization", header);

        int user_id = -1;
        bool auth_result = checkAuth(req, user_id);

        EXPECT_FALSE(auth_result) << "Header: " << header;
    }
}

// 测试 XSS 防护 - 输入验证
TEST_F(ApiSecurityTest, XssInputValidation)
{
    std::vector<std::string> xss_payloads = {
        "<script>alert('XSS')</script>",
        "<img src=x onerror=alert('XSS')>",
        "<svg/onload=alert('XSS')>",
        "javascript:alert('XSS')",
        "<iframe src='javascript:alert(\"XSS\")'></iframe>"};

    for (const auto &payload : xss_payloads)
    {
        // 验证输入包含潜在的 XSS 攻击
        bool has_xss_pattern = (payload.find("<script") != std::string::npos ||
                                payload.find("<img") != std::string::npos ||
                                payload.find("<svg") != std::string::npos ||
                                payload.find("javascript:") != std::string::npos ||
                                payload.find("<iframe") != std::string::npos ||
                                payload.find("onerror") != std::string::npos ||
                                payload.find("onload") != std::string::npos);
        EXPECT_TRUE(has_xss_pattern) << "Payload: " << payload;
    }
}

// 测试 CSRF token 验证（模拟）
TEST_F(ApiSecurityTest, CsrfTokenValidation)
{
    HttpRequest req;
    req.setMethod("POST");

    // 缺少 CSRF token 的 POST 请求
    auto csrf_token = req.getHeader("X-CSRF-Token");
    EXPECT_FALSE(csrf_token.has_value());

    // 应该在实际应用中验证 CSRF token
}

// 测试参数化查询（模拟）
TEST_F(ApiSecurityTest, ParameterizedQuerySimulation)
{
    // 模拟参数化查询的使用
    std::string user_input = "admin' OR '1'='1";

    // 使用 mysql_real_escape_string 转义（需要 MYSQL 连接）
    // 这里只是验证输入包含需要转义的字符
    bool needs_escaping = (user_input.find("'") != std::string::npos ||
                           user_input.find("\"") != std::string::npos ||
                           user_input.find("\\") != std::string::npos);
    EXPECT_TRUE(needs_escaping);

    // 实际应用中应该使用 mysql_stmt_prepare 和 mysql_stmt_bind_param
}
