#include "LogFile.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <algorithm>

/**
 * @brief LogFile 构造函数
 * @param basename 日志文件的基本名称，包含路径
 * @param rollSize 日志文件滚动大小阈值（字节）
 * @param flushInterval 刷新间隔（秒）
 * @param adaptiveFlush 是否启用自适应刷新
 * @param enableLevelFlush 是否启用分级刷新策略
 */
LogFile::LogFile(const std::string &basename, off_t rollSize, int flushInterval,
                 bool adaptiveFlush, bool enableLevelFlush)
    : m_basename(basename),
      m_rollSize(rollSize),
      m_flushInterval(flushInterval),
      m_adaptiveFlush(adaptiveFlush),
      m_enableLevelFlush(enableLevelFlush),
      m_count(0),
      m_writeRate(0),
      m_lastRateUpdate(0),
      m_mutex(new std::mutex),
      m_startOfPeriod(0),
      m_lastRoll(0),
      m_lastFlush(0)
{
    // 确保日志目录存在
    size_t pos = basename.find_last_of('/');
    if (pos != std::string::npos)
    {
        std::string dir = basename.substr(0, pos);
        // 使用系统命令创建目录，确保目录存在
        std::string cmd = "mkdir -p " + dir;
        system(cmd.c_str());
    }

    // 初始化各级别的刷新间隔
    m_levelFlushInterval[Level::DEBUG] = flushInterval;
    m_levelFlushInterval[Level::INFO] = flushInterval;
    m_levelFlushInterval[Level::WARN] = flushInterval / 2; // 警告级别刷新更快
    m_levelFlushInterval[Level::ERROR] = 0;                // 错误级别立即刷新
    m_levelFlushInterval[Level::FATAL] = 0;                // 致命错误立即刷新

    rollFile(); // 第一次创建时就生成一个日志文件
}

/**
 * @brief 析构函数，关闭文件
 */
LogFile::~LogFile()
{
    if (m_file)
    {
        ::fclose(m_file);
    }
}

/**
 * @brief 向日志文件追加日志内容（线程安全版本）
 * @param logline 日志内容
 * @param len 日志长度
 * @param level 日志级别，用于分级刷新策略
 */
void LogFile::append(const char *logline, int len, Level level)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    append_unlocked(logline, len, level);
}

/**
 * @brief 刷新日志文件缓冲区到磁盘（线程安全版本）
 */
void LogFile::flush()
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    ::fflush(m_file);
}

/**
 * @brief 设置特定级别的刷新间隔
 * @param level 日志级别
 * @param interval 刷新间隔(秒)
 */
void LogFile::setLevelFlushInterval(Level level, int interval)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_levelFlushInterval[level] = interval;
}

/**
 * @brief 计算当前适合的刷新间隔
 * @return 刷新间隔(秒)
 */
int LogFile::calculateAdaptiveInterval() const
{
    // 根据写入速率动态调整刷新间隔
    if (m_writeRate <= 10)
    {
        // 低频写入，使用较长间隔
        return std::min(m_flushInterval * 2, 5); // 最长5秒
    }
    else if (m_writeRate <= 100)
    {
        // 中频写入，使用默认间隔
        return m_flushInterval;
    }
    else if (m_writeRate <= 1000)
    {
        // 高频写入，使用较短间隔
        return std::max(m_flushInterval / 2, 1); // 最短1秒
    }
    else
    {
        // 超高频写入，使用最短间隔
        return 1;
    }
}

/**
 * @brief 向日志文件追加日志内容（非线程安全版本）
 * @param logline 日志内容
 * @param len 日志长度
 * @param level 日志级别
 * @note 该函数假设调用者已经获取了互斥锁
 */
void LogFile::append_unlocked(const char *logline, int len, Level level)
{
    // 尝试写入日志内容
    size_t n = ::fwrite(logline, 1, len, m_file);
    size_t remain = len - n;

    // 如果一次没写完，循环写入剩余部分
    while (remain > 0)
    {
        size_t x = ::fwrite(logline + n, 1, remain, m_file);
        if (x == 0)
        {
            int err = ferror(m_file);
            if (err)
            {
                // 文件写入错误处理
                char errBuf[128];
                strerror_r(err, errBuf, sizeof(errBuf));

                // 1. 输出错误到标准错误流
                fprintf(stderr, "[LogFile] 写入文件 %s 失败: %s\n",
                        m_basename.c_str(), errBuf);

                // 2. 尝试重新打开文件
                ::fclose(m_file);
                m_file = ::fopen(getLogFileName(m_basename, &m_lastRoll).c_str(), "ae");

                // 3. 如果重新打开失败，尝试创建备用文件
                if (!m_file)
                {
                    std::string backupFile = m_basename + ".error.log";
                    m_file = ::fopen(backupFile.c_str(), "ae");
                    if (m_file)
                    {
                        fprintf(stderr, "[LogFile] 已切换到备用日志文件: %s\n", backupFile.c_str());
                    }
                    else
                    {
                        fprintf(stderr, "[LogFile] 严重错误: 无法创建任何日志文件\n");
                    }
                }
            }
            break;
        }
        n += x;
        remain = len - n;
    }

    // 累计写入的字节数
    m_count += len;

    // 更新写入速率统计
    time_t now = ::time(nullptr);
    if (m_lastRateUpdate == 0)
    {
        m_lastRateUpdate = now;
        m_writeRate = 1;
    }
    else if (now != m_lastRateUpdate)
    {
        // 简单的速率计算：每秒写入的日志条数
        m_writeRate = (m_writeRate + 1) / (now - m_lastRateUpdate);
        m_lastRateUpdate = now;
    }
    else
    {
        // 同一秒内多次写入
        m_writeRate++;
    }

    // 记录该级别最后写入时间
    m_levelLastWrite[level] = now;

    // 判断是否需要刷新
    bool needFlush = false;

    // 1. 检查分级刷新策略
    if (m_enableLevelFlush)
    {
        int levelInterval = m_levelFlushInterval[level];
        // 如果该级别配置为立即刷新(interval=0)或达到该级别的刷新间隔
        if (levelInterval == 0 || (now - m_lastFlush >= levelInterval))
        {
            needFlush = true;
        }
    }

    // 2. 检查自适应刷新策略
    if (!needFlush && m_adaptiveFlush)
    {
        int adaptiveInterval = calculateAdaptiveInterval();
        if (now - m_lastFlush >= adaptiveInterval)
        {
            needFlush = true;
        }
    }

    // 3. 如果前两种策略都不需要刷新，检查基本刷新间隔
    if (!needFlush && now - m_lastFlush >= m_flushInterval)
    {
        needFlush = true;
    }

    // 执行刷新
    if (needFlush)
    {
        ::fflush(m_file);
        m_lastFlush = now;
    }

    // 如果累计写入量超过滚动阈值，则滚动日志文件
    if (m_count > m_rollSize)
    {
        rollFile();
    }
}

/**
 * @brief 滚动日志文件
 * @details 创建新的日志文件，关闭旧文件
 * @note 该函数假设调用者已经获取了互斥锁
 */
void LogFile::rollFile()
{
    time_t now = 0;
    // 生成新的日志文件名
    std::string filename = getLogFileName(m_basename, &now);

    // 计算当天开始时间（秒）
    // 一天有86400秒
    time_t start = now / 86400 * 86400;

    if (now > m_lastRoll)
    {
        m_lastRoll = now;        // 更新最后滚动时间
        m_lastFlush = now;       // 更新最后刷新时间
        m_startOfPeriod = start; // 更新当前周期开始时间
        m_count = 0;             // 重置计数器

        // 关闭旧文件（如果存在）
        if (m_file)
        {
            ::fclose(m_file);
        }

        // 打开新文件，使用"ae"模式：
        // a: 追加模式
        // e: O_CLOEXEC标志，在exec调用时关闭文件描述符
        m_file = ::fopen(filename.c_str(), "ae");
    }
}

/**
 * @brief 生成日志文件名
 * @param basename 基本文件名
 * @param now 当前时间指针，函数会更新此值
 * @return 完整的日志文件名
 * @details 文件名格式：basename.YYYYmmdd-HHMMSS.hostname.pid.log
 */
std::string LogFile::getLogFileName(const std::string &basename, time_t *now)
{
    std::string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL);
    localtime_r(now, &tm);
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
    filename += timebuf;

    char hostname[256];
    if (::gethostname(hostname, sizeof(hostname)) == 0)
    {
        hostname[sizeof(hostname) - 1] = '\0';
        filename += hostname;
    }
    else
    {
        filename += "unknownhost";
    }

    char pidbuf[32];
    snprintf(pidbuf, sizeof pidbuf, ".%d", ::getpid());
    filename += pidbuf;
    filename += ".log";

    return filename;
}