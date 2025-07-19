#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include "base/noncopyable.h"
#include "LogLevel.h"

/**
 * @brief 日志文件类，负责文件写入、滚动及刷新
 *
 * LogFile专注于日志的文件操作，包括：
 * 1. 文件的创建和关闭
 * 2. 数据的写入和刷新
 * 3. 日志的滚动(当文件大小超过阈值或到达时间间隔)
 * 4. 自适应刷新间隔和分级刷新策略
 *
 * 它作为AsyncLogging的底层支持，处理真正的文件I/O操作
 */
class LogFile : private noncopyable
{
public:
    using ptr = std::shared_ptr<LogFile>;

    /**
     * @brief 日志滚动模式枚举
     */
    enum class RollMode
    {
        SIZE,         // 按大小滚动
        DAILY,        // 每天滚动一次
        HOURLY,       // 每小时滚动一次
        MINUTELY,     // 每分钟滚动一次（仅用于测试）
        SIZE_DAILY,   // 综合策略：按大小和每天滚动
        SIZE_HOURLY,  // 综合策略：按大小和每小时滚动
        SIZE_MINUTELY // 综合策略：按大小和每分钟滚动（仅用于测试）
    };

    /**
     * @brief 构造函数
     * @param basename 日志文件基础名称，如"server_log"
     * @param rollSize 日志文件滚动的阈值大小(字节)
     * @param rollMode 日志滚动模式，默认按大小滚动
     * @param flushInterval 定期刷新的时间间隔(秒)
     * @param adaptiveFlush 是否启用自适应刷新
     * @param enableLevelFlush 是否启用分级刷新策略
     */
    LogFile(const std::string &basename,
            off_t rollSize,
            RollMode rollMode = RollMode::SIZE,
            int flushInterval = 1,
            bool adaptiveFlush = true,
            bool enableLevelFlush = true);
    ~LogFile();

    /**
     * @brief 写入日志
     * @param logline 日志内容
     * @param len 日志长度
     * @param level 日志级别，用于分级刷新策略
     *
     * 写入一行日志，根据需要自动触发文件滚动和缓冲刷新
     */
    void append(const char *logline, int len, Level level = Level::INFO);

    /**
     * @brief 刷新文件缓冲区到磁盘
     */
    void flush();

    /**
     * @brief 执行日志滚动，创建新文件
     *
     * 当文件大小超过rollSize或达到特定时间点时触发
     */
    void rollFile();

    /**
     * @brief 设置特定级别的刷新间隔
     * @param level 日志级别
     * @param interval 刷新间隔(秒)
     */
    void setLevelFlushInterval(Level level, int interval);

    /**
     * @brief 设置是否启用自适应刷新
     * @param enable 是否启用
     */
    void setAdaptiveFlush(bool enable) { m_adaptiveFlush = enable; }

    /**
     * @brief 设置是否启用分级刷新
     * @param enable 是否启用
     */
    void setLevelFlush(bool enable) { m_enableLevelFlush = enable; }

    /**
     * @brief 设置日志滚动模式
     * @param mode 滚动模式
     */
    void setRollMode(RollMode mode) { m_rollMode = mode; }

    /**
     * @brief 获取当前滚动模式
     * @return 当前滚动模式
     */
    RollMode getRollMode() const { return m_rollMode; }

private:
    /**
     * @brief 生成日志文件名
     * @param basename 日志基础名称
     * @param now 当前时间指针
     * @return 带有时间戳的完整文件名
     */
    static std::string getLogFileName(const std::string &basename, time_t *now);

    /**
     * @brief 无锁版本的append，内部使用
     * @param logline 日志内容
     * @param len 日志长度
     * @param level 日志级别
     */
    void append_unlocked(const char *logline, int len, Level level);

    /**
     * @brief 计算当前适合的刷新间隔
     * @return 刷新间隔(秒)
     */
    int calculateAdaptiveInterval() const;

    /**
     * @brief 检查是否需要按时间滚动
     * @param now 当前时间
     * @return 是否需要滚动
     */
    bool shouldRollByTime(time_t now) const;

private:
    const std::string m_basename; // 日志文件基础名称
    const off_t m_rollSize;       // 日志文件滚动阈值(字节)
    RollMode m_rollMode;          // 日志滚动模式
    int m_flushInterval;          // 基础刷新间隔(秒)
    bool m_adaptiveFlush;         // 是否启用自适应刷新
    bool m_enableLevelFlush;      // 是否启用分级刷新

    int m_count;             // 写入计数器，用于判断是否需要滚动
    int m_writeRate;         // 写入速率(条/秒)，用于自适应刷新
    time_t m_lastRateUpdate; // 上次更新写入速率的时间

    std::unique_ptr<std::mutex> m_mutex; // 互斥锁，保护文件操作
    time_t m_startOfPeriod;              // 当前日志周期的开始时间(天)
    time_t m_lastRoll;                   // 上次滚动的时间
    time_t m_lastFlush;                  // 上次刷新的时间

    // 按时间滚动相关
    int m_lastDay;    // 上次滚动的日期(天)
    int m_lastHour;   // 上次滚动的小时
    int m_lastMinute; // 上次滚动的分钟

    // 各级别最后写入时间和刷新间隔
    std::unordered_map<Level, time_t> m_levelLastWrite;  // 各级别最后写入时间
    std::unordered_map<Level, int> m_levelFlushInterval; // 各级别刷新间隔(秒)

    FILE *m_file; // 文件指针
};