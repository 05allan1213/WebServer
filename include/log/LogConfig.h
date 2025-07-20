#pragma once
#include "base/BaseConfig.h"

class LogConfig : public BaseConfig
{
public:
    void load(const std::string &filename) override;

    // 日志配置接口
    std::string getBasename() const;
    int getRollSize() const;
    int getFlushInterval() const;
    std::string getRollMode() const;
    bool getEnableFile() const;
    std::string getFileLevel() const;
    std::string getConsoleLevel() const;

    static LogConfig &getInstance();

private:
    LogConfig() = default;
    static LogConfig instance_;
};