#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 日志配置类,管理日志系统的配置参数
 *
 * 负责解析和提供 'log' 部分的配置。
 */
class LogConfig : noncopyable
{
public:
    /**
     * @brief 构造函数, 从 YAML 节点解析配置
     * @param node 包含日志配置的 YAML 节点
     */
    explicit LogConfig(const YAML::Node &node);

    std::string getBasename() const;
    int getRollSize() const;
    int getFlushInterval() const;
    std::string getRollMode() const;
    bool getEnableFile() const;
    std::string getFileLevel() const;
    std::string getConsoleLevel() const;
    bool getEnableAsync() const;

private:
    void validateConfig(const std::string &basename, int rollSize, int flushInterval,
                        const std::string &rollMode, const std::string &fileLevel,
                        const std::string &consoleLevel);
    YAML::Node node_;
};