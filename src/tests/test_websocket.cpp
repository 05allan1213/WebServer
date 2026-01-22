#include <gtest/gtest.h>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/HttpParser.h"
#include "websocket/WebSocketParser.h"
#include "websocket/WebSocketHandler.h"
#include "base/Buffer.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cstring>

// Base64 编码辅助函数
std::string base64Encode(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);

    return result;
}

// 计算 WebSocket Accept key
std::string calculateWebSocketAccept(const std::string &key)
{
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(combined.c_str()), combined.length(), hash);

    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

class WebSocketTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        parser = std::make_unique<HttpParser>();
        buffer = std::make_unique<Buffer>();
        ws_parser = std::make_unique<WebSocketParser>();
    }

    std::unique_ptr<HttpParser> parser;
    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<WebSocketParser> ws_parser;
};

// 测试 WebSocket 升级请求
TEST_F(WebSocketTest, WebSocketUpgradeRequest)
{
    std::string request =
        "GET /ws HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    EXPECT_EQ(req.getMethod(), HttpRequest::Method::kGet);
    EXPECT_EQ(req.getPath(), "/ws");

    auto upgrade = req.getHeader("upgrade");
    ASSERT_TRUE(upgrade.has_value());
    EXPECT_EQ(upgrade.value(), "websocket");

    auto connection = req.getHeader("connection");
    ASSERT_TRUE(connection.has_value());
    EXPECT_EQ(connection.value(), "Upgrade");

    auto ws_key = req.getHeader("sec-websocket-key");
    ASSERT_TRUE(ws_key.has_value());
    EXPECT_EQ(ws_key.value(), "dGhlIHNhbXBsZSBub25jZQ==");

    auto ws_version = req.getHeader("sec-websocket-version");
    ASSERT_TRUE(ws_version.has_value());
    EXPECT_EQ(ws_version.value(), "13");
}

// 测试 WebSocket 升级响应
TEST_F(WebSocketTest, WebSocketUpgradeResponse)
{
    std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string expected_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

    std::string calculated_accept = calculateWebSocketAccept(client_key);
    EXPECT_EQ(calculated_accept, expected_accept);

    HttpResponse resp(false);
    resp.setStatusCode(HttpResponse::k101SwitchingProtocols);
    resp.setHeader("Upgrade", "websocket");
    resp.setHeader("Connection", "Upgrade");
    resp.setHeader("Sec-WebSocket-Accept", calculated_accept);

    EXPECT_EQ(resp.getStatusCode(), HttpResponse::k101SwitchingProtocols);
}

// 测试缺少必需头部的 WebSocket 升级请求
TEST_F(WebSocketTest, WebSocketUpgradeMissingHeaders)
{
    // 缺少 Sec-WebSocket-Key
    std::string request =
        "GET /ws HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    auto ws_key = req.getHeader("sec-websocket-key");
    EXPECT_FALSE(ws_key.has_value());
}

// 测试错误的 WebSocket 版本
TEST_F(WebSocketTest, WebSocketWrongVersion)
{
    std::string request =
        "GET /ws HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 8\r\n" // 错误的版本
        "\r\n";

    buffer->append(request.data(), request.size());
    ASSERT_TRUE(parser->parseRequest(buffer.get()));
    ASSERT_TRUE(parser->gotAll());

    const auto &req = parser->request();
    auto ws_version = req.getHeader("sec-websocket-version");
    ASSERT_TRUE(ws_version.has_value());
    EXPECT_NE(ws_version.value(), "13");
}

// 测试 WebSocket 文本帧编码
TEST_F(WebSocketTest, WebSocketTextFrameEncoding)
{
    std::string payload = "Hello WebSocket";
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::TEXT_FRAME,
        payload,
        true,  // fin
        false  // not masked
    );

    EXPECT_FALSE(frame.empty());
    // 第一个字节应该是 0x81 (FIN=1, opcode=1)
    EXPECT_EQ(static_cast<unsigned char>(frame[0]), 0x81);
}

// 测试 WebSocket Ping 帧
TEST_F(WebSocketTest, WebSocketPingFrame)
{
    std::string payload = "ping";
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::PING,
        payload,
        true,
        false);

    EXPECT_FALSE(frame.empty());
    // 第一个字节应该是 0x89 (FIN=1, opcode=9)
    EXPECT_EQ(static_cast<unsigned char>(frame[0]), 0x89);
}

// 测试 WebSocket Pong 帧
TEST_F(WebSocketTest, WebSocketPongFrame)
{
    std::string payload = "pong";
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::PONG,
        payload,
        true,
        false);

    EXPECT_FALSE(frame.empty());
    // 第一个字节应该是 0x8A (FIN=1, opcode=10)
    EXPECT_EQ(static_cast<unsigned char>(frame[0]), 0x8A);
}

// 测试 WebSocket 关闭帧
TEST_F(WebSocketTest, WebSocketCloseFrame)
{
    std::string payload = "";
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::CONNECTION_CLOSE,
        payload,
        true,
        false);

    EXPECT_FALSE(frame.empty());
    // 第一个字节应该是 0x88 (FIN=1, opcode=8)
    EXPECT_EQ(static_cast<unsigned char>(frame[0]), 0x88);
}

// 测试 WebSocket 帧解析
TEST_F(WebSocketTest, WebSocketFrameParsing)
{
    std::string payload = "Test message";
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::TEXT_FRAME,
        payload,
        true,
        false);

    Buffer buf;
    buf.append(frame.data(), frame.size());

    bool frame_received = false;
    std::string received_payload;
    WebSocketParser::Opcode received_opcode;

    auto result = ws_parser->parse(&buf, [&](WebSocketParser::Opcode opcode, const std::string &data)
                                   {
        frame_received = true;
        received_opcode = opcode;
        received_payload = data; });

    EXPECT_EQ(result, WebSocketParser::OK);
    EXPECT_TRUE(frame_received);
    EXPECT_EQ(received_opcode, WebSocketParser::TEXT_FRAME);
    EXPECT_EQ(received_payload, payload);
}

// 测试 WebSocket 大负载帧
TEST_F(WebSocketTest, WebSocketLargePayload)
{
    std::string large_payload(65536, 'A'); // 64KB
    std::string frame = WebSocketParser::encodeFrame(
        WebSocketParser::BINARY_FRAME,
        large_payload,
        true,
        false);

    EXPECT_FALSE(frame.empty());
    EXPECT_GT(frame.size(), large_payload.size());
}

// 测试 WebSocket 分片帧
TEST_F(WebSocketTest, WebSocketFragmentedFrames)
{
    // 第一个分片 (FIN=0)
    std::string frame1 = WebSocketParser::encodeFrame(
        WebSocketParser::TEXT_FRAME,
        "Hello ",
        false, // not final
        false);

    // 延续帧 (FIN=1)
    std::string frame2 = WebSocketParser::encodeFrame(
        WebSocketParser::CONTINUATION,
        "World",
        true, // final
        false);

    EXPECT_FALSE(frame1.empty());
    EXPECT_FALSE(frame2.empty());

    // 第一个分片的 FIN 位应该是 0
    EXPECT_EQ(static_cast<unsigned char>(frame1[0]) & 0x80, 0x00);
    // 延续帧的 FIN 位应该是 1
    EXPECT_EQ(static_cast<unsigned char>(frame2[0]) & 0x80, 0x80);
}
