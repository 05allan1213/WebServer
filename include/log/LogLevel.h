#pragma once

#include <string>

/**
 * @brief 日志级别枚举，定义了日志的不同重要程度
 * 按照严重性从低到高排序：DEBUG < INFO < WARN < ERROR < FATAL
 * 当Logger设置某一级别后，只会输出大于等于该级别的日志
 */
enum class Level
{
    UNKNOW = 0, // 未知级别
    DEBUG = 1,  // 调试信息，用于开发阶段的详细追踪
    INFO = 2,   // 普通信息，记录系统正常运行状态
    WARN = 3,   // 警告信息，表示可能存在的问题
    ERROR = 4,  // 错误信息，表示发生了错误但程序仍可运行
    FATAL = 5,  // 严重错误信息，表示发生了致命错误
};

/**
 * @brief 将日志级别转换为字符串表示
 * @param level 日志级别
 * @return 对应的字符串表示
 */
std::string LogLevelToString(Level level);