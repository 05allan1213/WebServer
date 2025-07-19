#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);

public:
    // 获取当前的系统时间
    static Timestamp now();
    // 时间戳转字符串
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};