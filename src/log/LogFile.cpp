#include "LogFile.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>

// LogFile 构造函数
LogFile::LogFile(const std::string &basename, off_t rollSize, int flushInterval)
    : m_basename(basename),
      m_rollSize(rollSize),
      m_flushInterval(flushInterval),
      m_count(0),
      m_mutex(new std::mutex),
      m_startOfPeriod(0),
      m_lastRoll(0),
      m_lastFlush(0)
{
    // 确保目录存在
    size_t pos = basename.find_last_of('/');
    if (pos != std::string::npos)
    {
        std::string dir = basename.substr(0, pos);
        // 使用系统命令创建目录，确保目录存在
        std::string cmd = "mkdir -p " + dir;
        system(cmd.c_str());
    }

    rollFile(); // 第一次创建时就生成一个日志文件
}

LogFile::~LogFile()
{
    if (m_file)
    {
        ::fclose(m_file);
    }
}

// 核心追加函数，带锁
void LogFile::append(const char *logline, int len)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    append_unlocked(logline, len);
}

// 刷新，带锁
void LogFile::flush()
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    ::fflush(m_file);
}

// 不带锁的追加函数
void LogFile::append_unlocked(const char *logline, int len)
{
    size_t n = ::fwrite(logline, 1, len, m_file);
    size_t remain = len - n;
    while (remain > 0)
    { // 循环确保全部写入
        size_t x = ::fwrite(logline + n, 1, remain, m_file);
        if (x == 0)
        {
            int err = ferror(m_file);
            if (err)
            {
                // ... 错误处理 ...
            }
            break;
        }
        n += x;
        remain = len - n;
    }
    m_count += len;

    if (m_count > m_rollSize)
    {
        rollFile();
    }
}

// 滚动文件
void LogFile::rollFile()
{
    time_t now = 0;
    std::string filename = getLogFileName(m_basename, &now);

    // a day has 86400 seconds
    time_t start = now / 86400 * 86400; // a day

    if (now > m_lastRoll)
    {
        m_lastRoll = now;
        m_lastFlush = now;
        m_startOfPeriod = start;
        m_count = 0;
        if (m_file)
        {
            ::fclose(m_file);
        }
        m_file = ::fopen(filename.c_str(), "ae"); // 'e' for O_CLOEXEC
    }
}

// 生成文件名
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
    if (::gethostname(hostname, sizeof hostname) == 0)
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