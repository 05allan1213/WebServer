#include "AsyncLogging.h"
#include <cassert>
#include <chrono>

/**
 * @brief AsyncLogging构造函数
 * @param basename 日志文件基础名称
 * @param rollSize 日志文件滚动大小
 * @param flushInterval 刷新间隔（秒）
 * @param bufferSize 缓冲区大小
 * @details 初始化异步日志系统，创建双缓冲区和后台线程
 */
AsyncLogging::AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval, size_t bufferSize)
    : m_flushInterval(flushInterval),
      m_running(false),
      m_basename(basename),
      m_rollSize(rollSize),
      m_bufferSize(bufferSize),
      m_thread(),
      m_mutex(),
      m_cond(),
      m_currentBuffer(new Buffer(m_bufferSize)),
      m_nextBuffer(new Buffer(m_bufferSize)),
      m_buffers()
{
    m_currentBuffer->retrieveAll(); // 清空缓冲区
    m_nextBuffer->retrieveAll();
    m_buffers.reserve(16); // 预留空间，避免频繁扩容
}

/**
 * @brief AsyncLogging析构函数
 * @details 如果还在运行，则停止异步日志系统
 */
AsyncLogging::~AsyncLogging()
{
    if (m_running)
    {
        stop();
    }
}

/**
 * @brief 启动异步日志系统
 * @details 创建后台线程开始处理日志写入
 */
void AsyncLogging::start()
{
    m_running = true;
    m_thread = std::thread(&AsyncLogging::threadFunc, this);
}

/**
 * @brief 停止异步日志系统
 * @details 通知后台线程退出并等待其完成
 */
void AsyncLogging::stop()
{
    m_running = false;
    m_cond.notify_one(); // 唤醒后台线程，让其退出循环
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

/**
 * @brief 追加日志到缓冲区
 * @param logline 日志内容
 * @param len 日志长度
 * @details 使用双缓冲区技术，当前缓冲区满时切换到备用缓冲区
 */
void AsyncLogging::append(const char *logline, int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 如果当前缓冲区足够，直接写入
    if (m_currentBuffer->writableBytes() > len)
    {
        m_currentBuffer->append(logline, len);
    }
    else // 当前缓冲区满了
    {
        // 将当前缓冲区移入待写入队列
        m_buffers.push_back(std::move(m_currentBuffer));

        // 尝试使用备用缓冲区
        if (m_nextBuffer)
        {
            m_currentBuffer = std::move(m_nextBuffer);
        }
        else
        {
            // 如果备用缓冲区也被用了，就新分配一个（这种情况很少见）
            m_currentBuffer.reset(new Buffer(m_bufferSize));
        }

        // 将新日志写入新的当前缓冲区
        m_currentBuffer->append(logline, len);

        // 唤醒后台线程开始写入
        m_cond.notify_one();
    }
}

/**
 * @brief 从日志内容中提取日志级别
 * @param logline 日志内容
 * @param len 日志长度
 * @return 日志级别
 * @details 简单的日志级别提取，假设日志格式中包含如"[INFO]"这样的标记
 * 实际实现可能需要根据你的日志格式调整
 */
Level extractLogLevel(const char *logline, int len)
{
    // 简单的日志级别提取，假设日志格式中包含如"[INFO]"这样的标记
    // 实际实现可能需要根据你的日志格式调整
    std::string content(logline, len);
    if (content.find("[FATAL]") != std::string::npos)
        return Level::FATAL;
    if (content.find("[ERROR]") != std::string::npos)
        return Level::ERROR;
    if (content.find("[WARN]") != std::string::npos)
        return Level::WARN;
    if (content.find("[INFO]") != std::string::npos)
        return Level::INFO;
    if (content.find("[DEBUG]") != std::string::npos)
        return Level::DEBUG;

    return Level::INFO; // 默认级别
}

/**
 * @brief 后台线程函数
 * @details 负责将缓冲区中的日志写入文件，使用双缓冲区技术提高性能
 */
void AsyncLogging::threadFunc()
{
    assert(m_running == true);

    // 创建LogFile对象负责实际的文件操作
    // 启用自适应刷新和分级刷新策略
    LogFile output(m_basename, m_rollSize, LogFile::RollMode::SIZE, m_flushInterval, true, true);

    // 准备两个空闲缓冲区，用于与前端交换
    BufferPtr newBuffer1(new Buffer(m_bufferSize));
    BufferPtr newBuffer2(new Buffer(m_bufferSize));
    newBuffer1->retrieveAll();
    newBuffer2->retrieveAll();

    // 准备一个局部缓冲区队列，用于和前端的m_buffers交换
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    while (m_running)
    {
        {
            // 临界区开始
            std::unique_lock<std::mutex> lock(m_mutex);

            // 如果缓冲区队列为空，等待条件变量或超时
            if (m_buffers.empty())
            {
                m_cond.wait_for(lock, std::chrono::seconds(m_flushInterval));
            }

            // 将当前缓冲区移入待写入队列，即使它没满，也确保日志及时写入
            m_buffers.push_back(std::move(m_currentBuffer));

            // 用准备好的空缓冲区替换当前缓冲区
            m_currentBuffer = std::move(newBuffer1);

            // 快速交换前端和后端的缓冲区队列，减少锁的持有时间
            buffersToWrite.swap(m_buffers);

            // 如果备用缓冲区被前面用掉了，就补充一个
            if (!m_nextBuffer)
            {
                m_nextBuffer = std::move(newBuffer2);
            }
        } // 临界区结束，锁释放

        // 如果没有需要写入的日志，继续循环
        if (buffersToWrite.empty())
        {
            continue;
        }

        // 将所有缓冲区的数据写入日志文件
        for (const auto &buffer : buffersToWrite)
        {
            // 提取日志级别并传递给append方法
            const char *data = buffer->peek();
            size_t len = buffer->readableBytes();
            Level level = extractLogLevel(data, len);

            // 使用新的带级别参数的append方法
            output.append(data, len, level);
        }

        // 只保留两个缓冲区用于复用，减少内存分配
        if (buffersToWrite.size() > 2)
        {
            buffersToWrite.resize(2);
        }

        // 恢复newBuffer1，用于下一轮交换
        if (!newBuffer1)
        {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->retrieveAll(); // 清空内容但保留内存
        }

        // 恢复newBuffer2，用于下一轮交换
        if (!newBuffer2)
        {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->retrieveAll();
        }

        buffersToWrite.clear();
        // 不需要显式调用flush，LogFile的append会根据策略自动决定是否刷新
    }

    // 程序结束前确保所有缓冲区内容都写入并刷新
    output.flush();

    // 最后一次，将前端的缓冲区数据也写入
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_buffers.empty())
        {
            for (const auto &buffer : m_buffers)
            {
                const char *data = buffer->peek();
                size_t len = buffer->readableBytes();
                Level level = extractLogLevel(data, len);
                output.append(data, len, level);
            }
        }

        const char *data = m_currentBuffer->peek();
        size_t len = m_currentBuffer->readableBytes();
        Level level = extractLogLevel(data, len);
        output.append(data, len, level);

        output.flush();
    }
}