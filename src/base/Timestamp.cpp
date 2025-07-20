#include "base/Timestamp.h"
#include <ctime>
#include <cstdio> // for snprintf

// 必须在 .cpp 文件中,如果在头文件中,每个包含它的文件都会有一个副本
static_assert(sizeof(Timestamp) == sizeof(int64_t), "Timestamp should be int64_t");

Timestamp Timestamp::now()
{
    auto now_p = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now_p.time_since_epoch());
    return Timestamp(us.count());
}

std::string Timestamp::toString() const
{
    char buf[32] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%ld.%06ld", seconds, microseconds);
    return buf;
}

std::string Timestamp::toFormattedString(bool showMicroseconds) const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    struct tm tm_time;
    // 使用线程安全的 localtime_r
    localtime_r(&seconds, &tm_time);

    if (showMicroseconds)
    {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d.%06d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                 microseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}