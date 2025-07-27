#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief 日志配置类
 * @details 管理日志系统的配置参数，负责解析和提供'log'部分的配置
 */
class LogConfig : noncopyable
{
public:
    /**
     * @brief 构造函数
     * @param node 包含日志配置的YAML节点
     * @details 从YAML节点解析配置
     */
    explicit LogConfig(const YAML::Node &node);

    /**
     * @brief 获取日志文件名
     * @return 日志文件名
     */
    std::string getBasename() const;

    /**
     * @brief 获取日志文件滚动大小
     * @return 滚动大小（字节）
     */
    int getRollSize() const;

    /**
     * @brief 获取刷新间隔
     * @return 刷新间隔（秒）
     */
    int getFlushInterval() const;

    /**
     * @brief 获取滚动模式
     * @return 滚动模式字符串
     */
    std::string getRollMode() const;

    /**
     * @brief 获取是否启用文件输出
     * @return 是否启用文件输出
     */
    bool getEnableFile() const;

    /**
     * @brief 获取文件日志级别
     * @return 文件日志级别字符串
     */
    std::string getFileLevel() const;

    /**
     * @brief 获取控制台日志级别
     * @return 控制台日志级别字符串
     */
    std::string getConsoleLevel() const;

    /**
     * @brief 获取是否启用异步日志
     * @return 是否启用异步日志
     */
    bool getEnableAsync() const;

private:
    /**
     * @brief 验证配置参数
     * @param basename 日志文件名
     * @param rollSize 滚动大小
     * @param flushInterval 刷新间隔
     * @param rollMode 滚动模式
     * @param fileLevel 文件日志级别
     * @param consoleLevel 控制台日志级别
     */
    void validateConfig(const std::string &basename, int rollSize, int flushInterval,
                        const std::string &rollMode, const std::string &fileLevel,
                        const std::string &consoleLevel);

    YAML::Node node_; // YAML配置节点
};