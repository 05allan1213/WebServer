#pragma once

#include <memory>
#include <string>
#include <mutex>
#include "noncopyable.h"

/**
 * @brief 日志文件类，负责文件写入、滚动及刷新
 *
 * LogFile专注于日志的文件操作，包括：
 * 1. 文件的创建和关闭
 * 2. 数据的写入和刷新
 * 3. 日志的滚动(当文件大小超过阈值或到达时间间隔)
 *
 * 它作为AsyncLogging的底层支持，处理真正的文件I/O操作
 */
class LogFile : public noncopyable
{
public:
    using ptr = std::shared_ptr<LogFile>;

    /**
     * @brief 构造函数
     * @param basename 日志文件基础名称，如"server_log"
     * @param rollSize 日志文件滚动的阈值大小(字节)
     * @param flushInterval 定期刷新的时间间隔(秒)
     */
    LogFile(const std::string &basename,
            off_t rollSize,
            int flushInterval = 1);
    ~LogFile();

    /**
     * @brief 写入日志
     * @param logline 日志内容
     * @param len 日志长度
     *
     * 写入一行日志，根据需要自动触发文件滚动和缓冲刷新
     */
    void append(const char *logline, int len);

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
     */
    void append_unlocked(const char *logline, int len);

private:
    const std::string m_basename; // 日志文件基础名称
    const off_t m_rollSize;       // 日志文件滚动阈值(字节)
    const int m_flushInterval;    // 刷新间隔(秒)

    int m_count; // 写入计数器，用于判断是否需要滚动

    std::unique_ptr<std::mutex> m_mutex; // 互斥锁，保护文件操作
    time_t m_startOfPeriod;              // 当前日志周期的开始时间(天)
    time_t m_lastRoll;                   // 上次滚动的时间
    time_t m_lastFlush;                  // 上次刷新的时间

    FILE *m_file; // 文件指针
};