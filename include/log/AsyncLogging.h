#pragma once

#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Buffer.h"
#include "LogFile.h"
#include "noncopyable.h"

/**
 * @brief 异步日志类，实现高性能的日志后端
 *
 * AsyncLogging使用双缓冲(Double Buffering)技术和后台线程实现高效的异步日志:
 * 1. 前端线程只负责将日志快速写入内存缓冲区，几乎无阻塞
 * 2. 专门的后台线程负责将缓冲区中的日志写入磁盘
 * 3. 双缓冲机制最小化了锁竞争，提高了并发性能
 */
class AsyncLogging : public noncopyable
{
public:
    /**
     * @brief 构造函数
     * @param basename 日志文件基础名称
     * @param rollSize 日志滚动大小，超过此大小创建新日志文件
     * @param flushInterval 日志刷新间隔(秒)
     */
    AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval = 1);

    /**
     * @brief 析构函数，确保资源正确释放
     */
    ~AsyncLogging();

    /**
     * @brief 前端调用接口，向缓冲区添加一条日志
     * @param logline 日志内容
     * @param len 日志长度
     *
     * 此方法是线程安全的，可以被多个前端线程并发调用
     */
    void append(const char *logline, int len);

    /**
     * @brief 启动后台线程
     */
    void start();

    /**
     * @brief 停止后台线程
     */
    void stop();

private:
    /**
     * @brief 后台线程的工作函数
     *
     * 此函数负责将前端写入缓冲区的日志批量写入磁盘
     */
    void threadFunc();

    // 类型别名，提高代码可读性
    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    const int m_flushInterval;    // 刷新间隔(秒)
    std::atomic<bool> m_running;  // 线程运行标志
    const std::string m_basename; // 日志文件名
    const off_t m_rollSize;       // 日志文件滚动大小

    std::thread m_thread;           // 后台写入线程
    std::mutex m_mutex;             // 互斥锁，保护缓冲区
    std::condition_variable m_cond; // 条件变量，用于线程同步

    // 当前缓冲区，前端线程写入
    BufferPtr m_currentBuffer;
    // 备用缓冲区，当当前缓冲区满时使用
    BufferPtr m_nextBuffer;
    // 待写入文件的已满缓冲区列表
    BufferVector m_buffers;
};