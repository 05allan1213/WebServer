#pragma once

#include <iostream>
#include <string>

/**
 * @brief 时间戳类，封装微秒级时间戳
 *
 * Timestamp类封装了从Unix纪元开始的微秒级时间戳，提供了便捷的时间操作接口。
 * 主要功能：
 * - 获取当前系统时间
 * - 时间戳的创建和转换
 * - 时间戳的字符串表示
 *
 * 设计特点：
 * - 使用int64_t存储微秒级时间戳，精度高
 * - 提供静态方法获取当前时间
 * - 支持时间戳的字符串转换
 * - 不可变对象，线程安全
 *
 * 使用场景：
 * - 在EventLoop中记录事件发生时间
 * - 在TcpConnection中记录数据接收时间
 * - 需要高精度时间戳的场景
 */
class Timestamp
{
public:
    /**
     * @brief 默认构造函数
     *
     * 创建一个表示无效时间的时间戳对象
     */
    Timestamp();

    /**
     * @brief 构造函数
     * @param microSecondsSinceEpoch 从Unix纪元开始的微秒数
     *
     * 根据指定的微秒数创建时间戳对象
     */
    explicit Timestamp(int64_t microSecondsSinceEpoch);

public:
    /**
     * @brief 获取当前的系统时间
     * @return 当前时间的Timestamp对象
     *
     * 静态方法，获取系统当前时间并返回Timestamp对象
     */
    static Timestamp now();

    /**
     * @brief 将时间戳转换为字符串
     * @return 时间戳的字符串表示
     *
     * 将内部存储的微秒时间戳转换为可读的字符串格式
     */
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_; /**< 从Unix纪元开始的微秒数 */
};