#include <gtest/gtest.h>
#include "log/LogFormatter.h"
#include "log/LogEvent.h"
#include "log/LogLevel.h"
#include <memory>
#include <string>

TEST(LogFormatterTest, SimplePattern)
{
    LogFormatter fmt("%m%n");
    auto event = std::make_shared<LogEvent>("test.cpp", 10, 0, 123, 0, Level::INFO, "main");
    event->getStringStream() << "HelloLog";
    std::string result = fmt.format(nullptr, event);
    EXPECT_NE(result.find("HelloLog"), std::string::npos);
    EXPECT_NE(result.find("\n"), std::string::npos);
}

TEST(LogFormatterTest, LevelPattern)
{
    LogFormatter fmt("%p %m%n");
    auto event = std::make_shared<LogEvent>("test.cpp", 20, 0, 456, 0, Level::ERROR, "main");
    event->getStringStream() << "ErrorLog";
    std::string result = fmt.format(nullptr, event);
    EXPECT_NE(result.find("ERROR"), std::string::npos);
    EXPECT_NE(result.find("ErrorLog"), std::string::npos);
}

TEST(LogFormatterTest, ComplexPattern)
{
    LogFormatter fmt("%d{%Y-%m-%d} %p %m %n");
    auto event = std::make_shared<LogEvent>("test.cpp", 30, 0, 789, 0, Level::DEBUG, "main");
    event->getStringStream() << "DebugLog";
    std::string result = fmt.format(nullptr, event);
    EXPECT_NE(result.find("DebugLog"), std::string::npos);
    EXPECT_NE(result.find("DEBUG"), std::string::npos);
    EXPECT_NE(result.find("\n"), std::string::npos);
    // 日期格式可用正则进一步校验
}