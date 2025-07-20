#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

/**
 * @brief 网络地址封装类
 *
 * InetAddress封装了socket地址类型，提供了便捷的地址操作接口。
 * 主要功能包括：
 * - 封装sockaddr_in结构体
 * - 提供IP地址和端口的字符串转换
 * - 支持IPv4地址的创建和访问
 *
 * 这个类简化了网络编程中的地址处理，避免了直接操作sockaddr_in的复杂性。
 * 主要用于TcpServer的监听地址和TcpConnection的本地/对端地址。
 */
class InetAddress
{
public:
    /**
     * @brief 构造函数，通过端口和IP地址创建
     * @param port 端口号，0表示随机端口
     * @param ip IP地址字符串，默认为"127.0.0.1"
     *
     * 创建一个IPv4地址对象，支持点分十进制格式的IP地址。
     */
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");

    /**
     * @brief 构造函数，通过sockaddr_in结构体创建
     * @param addr 已初始化的sockaddr_in结构体
     *
     * 直接使用现有的sockaddr_in结构体创建InetAddress对象。
     */
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }

    /**
     * @brief 获取点分十进制格式的IP地址字符串
     * @return IP地址字符串，如"192.168.1.1"
     */
    std::string toIp() const;

    /**
     * @brief 获取"ip:port"格式的地址字符串
     * @return 地址字符串，如"192.168.1.1:8080"
     */
    std::string toIpPort() const;

    /**
     * @brief 获取端口号
     * @return 端口号（网络字节序）
     */
    uint16_t toPort() const;

    /**
     * @brief 获取当前对象封装的网络地址信息
     * @return sockaddr_in结构体的常量指针
     *
     * 返回内部sockaddr_in结构体的指针，用于socket系统调用。
     */
    const sockaddr_in *getSockAddr() const { return &addr_; }

    /**
     * @brief 设置当前对象封装的网络地址信息
     * @param addr 新的sockaddr_in结构体
     *
     * 更新内部存储的地址信息。
     */
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_; // 内部存储的socket地址结构体
};