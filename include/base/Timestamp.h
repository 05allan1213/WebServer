#pragma once

#include <chrono>
#include <string>
#include <algorithm>

/**
 * @brief 时间戳包装类，基于std::chrono::system_clock::time_point
 *
 * 提供高精度时间戳功能，支持微秒级精度的时间操作。
 */
class Timestamp
{
public:
    /**
     * @brief 默认构造函数，初始化为0
     */
    Timestamp() : microSecondsSinceEpoch_(0) {}

    /**
     * @brief 构造函数，从微秒时间戳构造
     * @param microSecondsSinceEpoch 微秒时间戳
     */
    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    /**
     * @brief 交换两个时间戳
     * @param that 要交换的时间戳
     */
    void swap(Timestamp &that)
    {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    /**
     * @brief 获取当前时间
     * @return 当前时间的时间戳
     */
    static Timestamp now();

    /**
     * @brief 转换为字符串格式
     * @return 格式化的时间字符串
     */
    std::string toString() const;

    /**
     * @brief 转换为格式化字符串
     * @param showMicroseconds 是否显示微秒
     * @return 格式化的时间字符串
     */
    std::string toFormattedString(bool showMicroseconds = true) const;

    /**
     * @brief 检查时间戳是否有效
     * @return 是否有效
     */
    bool valid() const { return microSecondsSinceEpoch_ > 0; }

    /**
     * @brief 获取微秒时间戳
     * @return 微秒时间戳
     */
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    /**
     * @brief 获取秒时间戳
     * @return 秒时间戳
     */
    time_t secondsSinceEpoch() const
    {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    }

    // 常量
    static const int kMicroSecondsPerSecond = 1000 * 1000; // 每秒微秒数

private:
    int64_t microSecondsSinceEpoch_; // 微秒时间戳
};

/**
 * @brief 小于比较操作符
 * @param lhs 左操作数
 * @param rhs 右操作数
 * @return 比较结果
 */
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

/**
 * @brief 相等比较操作符
 * @param lhs 左操作数
 * @param rhs 右操作数
 * @return 比较结果
 */
inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

/**
 * @brief 向时间戳添加秒数
 * @param timestamp 原始时间戳
 * @param seconds 要添加的秒数
 * @return 新的时间戳
 */
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}