#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

class Buffer;

/**
 * @brief WebSocket数据帧解析器
 */
class WebSocketParser
{
public:
    enum Opcode
    {
        CONTINUATION = 0x0,
        TEXT_FRAME = 0x1,
        BINARY_FRAME = 0x2,
        CONNECTION_CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xA,
    };

    enum ParseResult
    {
        INCOMPLETE,
        OK,
        ERROR,
    };

    using FrameCallback = std::function<void(Opcode, const std::string &)>;

    WebSocketParser();

    ParseResult parse(Buffer *buf, const FrameCallback &onFrame);

    static std::string encodeFrame(Opcode opcode, const std::string &payload, bool fin = true, bool masked = false);

private:
    enum State
    {
        READ_HEADER,
        READ_PAYLOAD_LENGTH_16,
        READ_PAYLOAD_LENGTH_64,
        READ_MASK,
        READ_PAYLOAD,
    };

    State state_;
    bool fin_;
    Opcode opcode_;
    uint64_t payload_len_;
    uint8_t mask_[4];
    bool masked_;
};