#include "AsyncLogging.h"
#include <cassert>
#include <chrono>

AsyncLogging::AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval)
    : m_flushInterval(flushInterval),
      m_running(false),
      m_basename(basename),
      m_rollSize(rollSize),
      m_thread(),
      m_mutex(),
      m_cond(),
      m_currentBuffer(new Buffer),
      m_nextBuffer(new Buffer),
      m_buffers()
{
    m_currentBuffer->retrieveAll(); // 清空缓冲区
    m_nextBuffer->retrieveAll();
    m_buffers.reserve(16); // 预留空间，避免频繁扩容
}

AsyncLogging::~AsyncLogging()
{
    if (m_running)
    {
        stop();
    }
}

void AsyncLogging::start()
{
    m_running = true;
    m_thread = std::thread(&AsyncLogging::threadFunc, this);
}

void AsyncLogging::stop()
{
    m_running = false;
    m_cond.notify_one(); // 唤醒后台线程，让其退出循环
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

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
            m_currentBuffer.reset(new Buffer);
        }

        // 将新日志写入新的当前缓冲区
        m_currentBuffer->append(logline, len);

        // 唤醒后台线程开始写入
        m_cond.notify_one();
    }
}

void AsyncLogging::threadFunc()
{
    assert(m_running == true);

    // 创建LogFile对象负责实际的文件操作
    LogFile output(m_basename, m_rollSize, 1);

    // 准备两个空闲缓冲区，用于与前端交换
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
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
            output.append(buffer->peek(), buffer->readableBytes());
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
        output.flush(); // 确保日志写入磁盘
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
                output.append(buffer->peek(), buffer->readableBytes());
            }
        }
        output.append(m_currentBuffer->peek(), m_currentBuffer->readableBytes());
        output.flush();
    }
}