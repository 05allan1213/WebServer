#include "websocket/WebSocketParser.h"
#include "base/Buffer.h"
#include <arpa/inet.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include "log/Log.h"

/**
 * @brief 构造函数，初始化WebSocket解析器
 * @details 设置初始状态为读取帧头，初始化所有成员变量
 */
WebSocketParser::WebSocketParser()
    : state_(READ_HEADER), fin_(false), opcode_(CONTINUATION), payload_len_(0), masked_(false)
{
    // 初始化状态机，准备解析第一个数据帧
}

/**
 * @brief 解析WebSocket数据帧
 * @param buf 输入缓冲区
 * @param onFrame 帧解析完成时的回调函数
 * @return 解析结果
 * @details 使用状态机解析WebSocket数据帧，支持分片传输和掩码处理
 */
WebSocketParser::ParseResult WebSocketParser::parse(Buffer *buf, const FrameCallback &onFrame)
{
    while (true)
    {
        switch (state_)
        {
        case READ_HEADER:
            // 检查是否有足够的数据读取帧头（至少2字节）
            if (buf->readableBytes() < 2)
                return INCOMPLETE;
            {
                const uint8_t *header = reinterpret_cast<const uint8_t *>(buf->peek());
                fin_ = (header[0] & 0x80) != 0;                  // 提取FIN标志位
                opcode_ = static_cast<Opcode>(header[0] & 0x0F); // 提取操作码
                masked_ = (header[1] & 0x80) != 0;               // 提取掩码标志位
                uint8_t len_byte = header[1] & 0x7F;             // 提取负载长度字节
                buf->retrieve(2);                                // 消费2字节帧头

                // 根据负载长度字节决定下一步状态
                if (len_byte < 126)
                {
                    // 负载长度小于126，直接使用该值
                    payload_len_ = len_byte;
                    state_ = masked_ ? READ_MASK : READ_PAYLOAD;
                }
                else if (len_byte == 126)
                {
                    // 负载长度为126，需要读取16位长度
                    state_ = READ_PAYLOAD_LENGTH_16;
                }
                else
                { // 127
                    // 负载长度为127，需要读取64位长度
                    state_ = READ_PAYLOAD_LENGTH_64;
                }
            }
            break;

        case READ_PAYLOAD_LENGTH_16:
            // 检查是否有足够的数据读取16位长度
            if (buf->readableBytes() < 2)
                return INCOMPLETE;
            {
                uint16_t len;
                memcpy(&len, buf->peek(), 2);
                payload_len_ = ntohs(len); // 网络字节序转换为主机字节序
                buf->retrieve(2);
                state_ = masked_ ? READ_MASK : READ_PAYLOAD;
            }
            break;

        case READ_PAYLOAD_LENGTH_64:
            // 检查是否有足够的数据读取64位长度
            if (buf->readableBytes() < 8)
                return INCOMPLETE;
            {
                // 手动转换大端序64位整数，提高可移植性
                const uint8_t *p = reinterpret_cast<const uint8_t *>(buf->peek());
                payload_len_ = (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 | (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
                               (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 | (uint64_t)p[6] << 8 | (uint64_t)p[7];
                buf->retrieve(8);
                state_ = masked_ ? READ_MASK : READ_PAYLOAD;
            }
            break;

        case READ_MASK:
            // 检查是否有足够的数据读取4字节掩码
            if (buf->readableBytes() < 4)
                return INCOMPLETE;
            memcpy(mask_, buf->peek(), 4); // 复制掩码数据
            buf->retrieve(4);
            state_ = READ_PAYLOAD;
            break;

        case READ_PAYLOAD:
            // 检查是否有足够的数据读取完整负载
            if (buf->readableBytes() < payload_len_)
                return INCOMPLETE;
            {
                std::string payload_data(buf->peek(), payload_len_);
                if (masked_)
                {
                    // 如果使用了掩码，需要解掩码处理
                    for (size_t i = 0; i < payload_data.size(); ++i)
                    {
                        payload_data[i] ^= mask_[i % 4]; // 按字节进行XOR操作
                    }
                }
                buf->retrieve(payload_len_); // 消费负载数据

                // 调用回调函数处理解析完成的帧
                onFrame(opcode_, payload_data);

                state_ = READ_HEADER; // 重置状态机，准备解析下一帧
                // 继续循环，尝试解析缓冲区中的更多帧
            }
            break;
        }
    }
    return OK; // 理论上应该在循环中处理完所有数据
}

/**
 * @brief 编码WebSocket数据帧
 * @param opcode 操作码
 * @param payload 负载数据
 * @param fin 是否为最后一帧
 * @param masked 是否使用掩码
 * @return 编码后的数据帧
 * @details 将数据编码为符合RFC 6455标准的WebSocket数据帧
 */
std::string WebSocketParser::encodeFrame(Opcode opcode, const std::string &payload, bool fin, bool masked)
{
    std::string frame;
    uint8_t header[10];                       // 最大帧头长度为10字节
    header[0] = (fin ? 0x80 : 0x00) | opcode; // 设置FIN标志位和操作码

    size_t len = payload.size();
    size_t header_len = 2; // 基础帧头长度为2字节

    // 根据负载长度设置长度字段
    if (len < 126)
    {
        header[1] = len; // 直接使用7位长度字段
    }
    else if (len < 65536)
    {
        header[1] = 126;             // 使用16位扩展长度
        uint16_t len16 = htons(len); // 转换为网络字节序
        memcpy(&header[2], &len16, 2);
        header_len += 2;
    }
    else
    {
        header[1] = 127;               // 使用64位扩展长度
        uint64_t len64 = htobe64(len); // 转换为大端序
        memcpy(&header[2], &len64, 8);
        header_len += 8;
    }

    if (masked)
    {
        header[1] |= 0x80; // 设置掩码标志位
        // 服务器到客户端的帧通常不加掩码，此处省略掩码生成
    }

    // 组装完整的数据帧
    frame.append(reinterpret_cast<const char *>(header), header_len);
    frame.append(payload);

    return frame;
}