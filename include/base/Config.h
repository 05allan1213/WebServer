#pragma once
#include <string>
#include <yaml-cpp/yaml.h>

class Config
{
public:
    static Config &getInstance();

    void load(const std::string &filename);

    // 日志配置
    std::string getLogBasename() const;
    int getLogRollSize() const;
    int getLogFlushInterval() const;
    std::string getLogRollMode() const;
    bool getLogEnableFile() const;
    std::string getLogFileLevel() const;
    std::string getLogConsoleLevel() const;

    // 网络配置
    int getNetworkPort() const;
    int getNetworkThreadNum() const;
    std::string getNetworkIp() const;

private:
    Config() = default;
    YAML::Node config_;
    static bool loaded_;
};
