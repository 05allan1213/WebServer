#include "LogLevel.h"
#include <string>

/**
 * @brief 将日志级别转换为字符串表示
 * @param level 日志级别枚举值
 * @return 日志级别的字符串表示
 */
std::string LogLevelToString(Level level)
{
    switch (level)
    {
#define XX(name)      \
    case Level::name: \
        return #name;

        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);

#undef XX
    default:
        return "UNKNOW";
    }
}