#pragma once
#include "base/BaseConfig.h"

/**
 * @brief 日志配置类，管理日志系统的配置参数
 *
 * LogConfig继承自BaseConfig，专门管理日志系统的配置参数。
 * 主要功能：
 * - 加载日志配置文件
 * - 提供日志相关的配置接口
 * - 支持配置参数的验证
 * - 管理日志文件的生命周期
 *
 * 设计特点：
 * - 单例模式，确保全局唯一的日志配置实例
 * - 继承BaseConfig，复用配置加载功能
 * - 提供类型安全的配置访问接口
 * - 支持配置参数的严格验证
 *
 * 配置参数包括：
 * - 文件配置：基础名称、滚动大小、刷新间隔
 * - 输出配置：文件输出开关、控制台输出级别
 * - 滚动配置：滚动模式、文件级别
 *
 * 支持的滚动模式：
 * - SIZE：按文件大小滚动
 * - TIME：按时间滚动
 * - SIZE_HOURLY：按大小和小时滚动
 *
 * 支持的日志级别：
 * - DEBUG：调试信息
 * - INFO：一般信息
 * - WARN：警告信息
 * - ERROR：错误信息
 * - FATAL：致命错误
 */
class LogConfig : public BaseConfig
{
public:
    /**
     * @brief 加载日志配置文件
     * @param filename 配置文件名
     *
     * 重写基类方法，加载日志配置文件并验证参数
     */
    void load(const std::string &filename) override;

    /**
     * @brief 获取日志文件基础名称
     * @return 日志文件的基础名称
     *
     * 日志文件的实际名称会在此基础上添加时间戳和序号
     */
    std::string getBasename() const;

    /**
     * @brief 获取日志文件滚动大小
     * @return 日志文件滚动大小（字节）
     *
     * 当日志文件达到此大小时，会创建新的日志文件
     */
    int getRollSize() const;

    /**
     * @brief 获取日志刷新间隔
     * @return 日志刷新间隔（秒）
     *
     * 定期将日志缓冲区的内容刷新到文件中
     */
    int getFlushInterval() const;

    /**
     * @brief 获取日志滚动模式
     * @return 日志滚动模式字符串
     *
     * 支持的模式：SIZE、TIME、SIZE_HOURLY
     */
    std::string getRollMode() const;

    /**
     * @brief 获取是否启用文件日志
     * @return 如果启用文件日志返回true，否则返回false
     */
    bool getEnableFile() const;

    /**
     * @brief 获取文件日志级别
     * @return 文件日志级别字符串
     *
     * 支持的级别：DEBUG、INFO、WARN、ERROR、FATAL
     */
    std::string getFileLevel() const;

    /**
     * @brief 获取控制台日志级别
     * @return 控制台日志级别字符串
     *
     * 支持的级别：DEBUG、INFO、WARN、ERROR、FATAL
     */
    std::string getConsoleLevel() const;

    /**
     * @brief 获取是否启用异步日志
     * @return 如果启用异步日志返回true，否则返回false
     */
    bool getEnableAsync() const;

    /**
     * @brief 获取LogConfig单例实例
     * @return LogConfig的引用
     *
     * 单例模式的访问接口，确保全局唯一的日志配置实例
     */
    static LogConfig &getInstance();

private:
    /**
     * @brief 私有构造函数
     *
     * 单例模式，禁止外部创建实例
     */
    LogConfig() = default;

    /** @brief 单例实例 */
    static LogConfig instance_;

    /**
     * @brief 配置参数验证方法
     * @param basename 日志文件基础名称
     * @param rollSize 滚动大小
     * @param flushInterval 刷新间隔
     * @param rollMode 滚动模式
     * @param fileLevel 文件日志级别
     * @param consoleLevel 控制台日志级别
     *
     * 验证配置参数的有效性，如果参数无效会抛出异常
     */
    void validateConfig(const std::string &basename, int rollSize, int flushInterval,
                        const std::string &rollMode, const std::string &fileLevel,
                        const std::string &consoleLevel);
};