#include <strings.h>
#include <string.h>
#include "InetAddress.h"

/**
 * @brief 构造函数
 * @param port 端口号，0表示随机端口
 * @param ip IP地址字符串，默认为"127.0.0.1"
 * @details 创建一个IPv4地址对象，支持点分十进制格式的IP地址
 */
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);                   // 清空地址结构体，确保所有字段为0
    addr_.sin_family = AF_INET;                    // 设置地址族为IPv4地址族
    addr_.sin_port = htons(port);                  // 设置端口号，转换为网络字节序（大端序）
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // 设置IP地址，将点分十进制字符串转换为网络字节序
}

/**
 * @brief 获取点分十进制格式的IP地址字符串
 * @return IP地址字符串，如"192.168.1.1"
 * @details 将网络字节序的IP地址转换为可读的点分十进制格式
 */
std::string InetAddress::toIp() const
{
    char buf[64] = {0}; // 缓冲区，足够存储IPv4地址字符串
    // 使用inet_ntop函数将网络字节序的IP地址转换为点分十进制字符串
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

/**
 * @brief 获取"ip:port"格式的地址字符串
 * @return 地址字符串，如"192.168.1.1:8080"
 * @details 将IP地址和端口号组合成标准的网络地址格式
 */
std::string InetAddress::toIpPort() const
{
    char buf[64] = {0}; // 缓冲区，足够存储"ip:port"格式的字符串
    // 首先转换IP地址为点分十进制格式
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);

    size_t end = strlen(buf);              // 获取IP地址字符串的长度
    uint16_t port = ntohs(addr_.sin_port); // 将网络字节序的端口号转换为主机字节序
    sprintf(buf + end, ":%u", port);       // 在IP地址后追加端口号，格式为":端口号"
    return buf;
}

/**
 * @brief 获取端口号
 * @return 端口号（主机字节序）
 * @details 将网络字节序的端口号转换为主机字节序并返回
 */
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port); // 网络字节序转主机字节序
}
