#include <strings.h>
#include <string.h>
#include "InetAddress.h"

// 根据端口号和IP地址初始化InetAddress对象
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);                   // 清空地址结构
    addr_.sin_family = AF_INET;                    // 设置地址族为IPv4地址族
    addr_.sin_port = htons(port);                  // 设置端口号,使用网络字节序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // 设置IP地址,使用网络字节序
}

// 获取点分十进制 IP 字符串
std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

// 获取 "ip:port" 格式的字符串
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);

    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
}

// 获取端口号
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}
