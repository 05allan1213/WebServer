#include "websocket/WebSocketParser.h"
#include "base/Buffer.h"
#include <arpa/inet.h> // for ntohs, htons
#include <stdexcept>
#include <algorithm>
#include <cstring> // for memcpy
#include "log/Log.h"

WebSocketParser::WebSocketParser()
    : state_(READ_HEADER), fin_(false), opcode_(CONTINUATION), payload_len_(0), masked_(false)
{
    // 构造函数，初始化状态机
}

WebSocketParser::ParseResult WebSocketParser::parse(Buffer *buf, const FrameCallback &onFrame)
{
    while (true)
    {
        switch (state_)
        {
        case READ_HEADER:
            if (buf->readableBytes() < 2)
                return INCOMPLETE;
            {
                const uint8_t *header = reinterpret_cast<const uint8_t *>(buf->peek());
                fin_ = (header[0] & 0x80) != 0;
                opcode_ = static_cast<Opcode>(header[0] & 0x0F);
                masked_ = (header[1] & 0x80) != 0;
                uint8_t len_byte = header[1] & 0x7F;
                buf->retrieve(2);

                if (len_byte < 126)
                {
                    payload_len_ = len_byte;
                    state_ = masked_ ? READ_MASK : READ_PAYLOAD;
                }
                else if (len_byte == 126)
                {
                    state_ = READ_PAYLOAD_LENGTH_16;
                }
                else
                { // 127
                    state_ = READ_PAYLOAD_LENGTH_64;
                }
            }
            break;

        case READ_PAYLOAD_LENGTH_16:
            if (buf->readableBytes() < 2)
                return INCOMPLETE;
            {
                uint16_t len;
                memcpy(&len, buf->peek(), 2);
                payload_len_ = ntohs(len);
                buf->retrieve(2);
                state_ = masked_ ? READ_MASK : READ_PAYLOAD;
            }
            break;

        case READ_PAYLOAD_LENGTH_64:
            if (buf->readableBytes() < 8)
                return INCOMPLETE;
            {
                // be64toh is not standard, use a manual conversion for portability
                const uint8_t *p = reinterpret_cast<const uint8_t *>(buf->peek());
                payload_len_ = (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 | (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
                               (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 | (uint64_t)p[6] << 8 | (uint64_t)p[7];
                buf->retrieve(8);
                state_ = masked_ ? READ_MASK : READ_PAYLOAD;
            }
            break;

        case READ_MASK:
            if (buf->readableBytes() < 4)
                return INCOMPLETE;
            memcpy(mask_, buf->peek(), 4);
            buf->retrieve(4);
            state_ = READ_PAYLOAD;
            break;

        case READ_PAYLOAD:
            if (buf->readableBytes() < payload_len_)
                return INCOMPLETE;
            {
                std::string payload_data(buf->peek(), payload_len_);
                if (masked_)
                {
                    for (size_t i = 0; i < payload_data.size(); ++i)
                    {
                        payload_data[i] ^= mask_[i % 4];
                    }
                }
                buf->retrieve(payload_len_);

                onFrame(opcode_, payload_data);

                state_ = READ_HEADER; // 重置状态机以解析下一帧
                // 继续循环，看是否能解析出更多帧
            }
            break;
        }
    }
    return OK; // 理论上应该在循环中处理完所有数据
}

std::string WebSocketParser::encodeFrame(Opcode opcode, const std::string &payload, bool fin, bool masked)
{
    std::string frame;
    uint8_t header[10];
    header[0] = (fin ? 0x80 : 0x00) | opcode;

    size_t len = payload.size();
    size_t header_len = 2;

    if (len < 126)
    {
        header[1] = len;
    }
    else if (len < 65536)
    {
        header[1] = 126;
        uint16_t len16 = htons(len);
        memcpy(&header[2], &len16, 2);
        header_len += 2;
    }
    else
    {
        header[1] = 127;
        uint64_t len64 = htobe64(len); // htobe64 might not be available on all systems
        memcpy(&header[2], &len64, 8);
        header_len += 8;
    }

    if (masked)
    {
        header[1] |= 0x80;
        // 服务器到客户端的帧不应加掩码，此处省略掩码生成
    }

    frame.append(reinterpret_cast<const char *>(header), header_len);
    frame.append(payload);

    return frame;
}