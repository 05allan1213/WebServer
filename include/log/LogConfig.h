#pragma once
#include "base/BaseConfig.h"

/**
 * @brief 日志配置类,管理日志系统的配置参数
 *
 * 负责加载、校验和访问日志相关配置项。
 * 支持单例模式，所有getter方法均支持缺省值和警告日志。
 */
class LogConfig : public BaseConfig
{
public:
    /**
     * @brief 加载日志配置文件
     * @param filename 配置文件名
     * @throws std::invalid_argument 配置无效时抛出
     */
    void load(const std::string &filename) override;

    /**
     * @brief 获取日志文件基础名称
     * @return 日志文件的基础名称
     */
    std::string getBasename() const;

    /**
     * @brief 获取日志文件滚动大小
     * @return 日志文件滚动大小(字节)
     */
    int getRollSize() const;

    /**
     * @brief 获取日志刷新间隔
     * @return 日志刷新间隔(秒)
     */
    int getFlushInterval() const;

    /**
     * @brief 获取日志滚动模式
     * @return 日志滚动模式字符串
     */
    std::string getRollMode() const;

    /**
     * @brief 获取是否启用文件日志
     * @return 如果启用文件日志返回true,否则返回false
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
     * @return 如果启用异步日志返回true,否则返回false
     */
    bool getEnableAsync() const;

    /**
     * @brief 获取LogConfig单例实例
     * @return LogConfig的引用
     */
    static LogConfig &getInstance();

private:
    /**
     * @brief 私有构造函数，单例模式
     */
    LogConfig() = default;

    /**
     * @brief 配置参数验证方法
     * @param basename 日志文件基础名称
     * @param rollSize 滚动大小
     * @param flushInterval 刷新间隔
     * @param rollMode 滚动模式
     * @param fileLevel 文件日志级别
     * @param consoleLevel 控制台日志级别
     * @throws std::invalid_argument 配置无效时抛出
     */
    void validateConfig(const std::string &basename, int rollSize, int flushInterval,
                        const std::string &rollMode, const std::string &fileLevel,
                        const std::string &consoleLevel);
};