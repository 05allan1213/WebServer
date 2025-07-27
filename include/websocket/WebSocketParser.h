#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

class Buffer;

/**
 * @brief WebSocket数据帧解析器
 * @details 负责解析WebSocket协议的数据帧，支持RFC 6455标准
 */
class WebSocketParser
{
public:
    /**
     * @brief WebSocket操作码枚举
     * @details 定义了WebSocket协议中的各种操作码
     */
    enum Opcode
    {
        CONTINUATION = 0x0,     // 延续帧
        TEXT_FRAME = 0x1,       // 文本帧
        BINARY_FRAME = 0x2,     // 二进制帧
        CONNECTION_CLOSE = 0x8, // 连接关闭帧
        PING = 0x9,             // Ping帧
        PONG = 0xA,             // Pong帧
    };

    /**
     * @brief 解析结果枚举
     * @details 表示数据帧解析的结果状态
     */
    enum ParseResult
    {
        INCOMPLETE, // 数据不完整，需要更多数据
        OK,         // 解析成功
        ERROR,      // 解析错误
    };

    /**
     * @brief 帧回调函数类型
     * @details 当解析完一个完整帧时调用的回调函数
     */
    using FrameCallback = std::function<void(Opcode, const std::string &)>;

    /**
     * @brief 构造函数
     * @details 初始化WebSocket解析器，设置初始状态
     */
    WebSocketParser();

    /**
     * @brief 解析WebSocket数据帧
     * @param buf 输入缓冲区
     * @param onFrame 帧解析完成时的回调函数
     * @return 解析结果
     * @details 从缓冲区中解析WebSocket数据帧，支持分片传输
     */
    ParseResult parse(Buffer *buf, const FrameCallback &onFrame);

    /**
     * @brief 编码WebSocket数据帧
     * @param opcode 操作码
     * @param payload 负载数据
     * @param fin 是否为最后一帧
     * @param masked 是否使用掩码
     * @return 编码后的数据帧
     * @details 将数据编码为WebSocket协议格式的数据帧
     */
    static std::string encodeFrame(Opcode opcode, const std::string &payload, bool fin = true, bool masked = false);

private:
    /**
     * @brief 解析状态枚举
     * @details 表示当前解析器所处的状态
     */
    enum State
    {
        READ_HEADER,            // 读取帧头
        READ_PAYLOAD_LENGTH_16, // 读取16位负载长度
        READ_PAYLOAD_LENGTH_64, // 读取64位负载长度
        READ_MASK,              // 读取掩码
        READ_PAYLOAD,           // 读取负载数据
    };

    State state_;          // 当前解析状态
    bool fin_;             // 是否为最后一帧
    Opcode opcode_;        // 当前帧的操作码
    uint64_t payload_len_; // 负载长度
    uint8_t mask_[4];      // 掩码数据
    bool masked_;          // 是否使用掩码
};