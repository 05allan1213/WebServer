#include "base/MemoryPool.h"
#include "log/Log.h"
#include <stdexcept>

MemoryPool &MemoryPool::getInstance()
{
    static MemoryPool instance;
    return instance;
}

MemoryPool::MemoryPool()
{
    // 初始化不同大小的内存池，例如 64B, 128B, ... 64KB
    for (size_t i = 64; i <= 65536; i *= 2)
    {
        poolSizes_.push_back(i);
        freeLists_[i] = {};
    }
}

MemoryPool::~MemoryPool()
{
    DLOG_INFO << "[MemoryPool] 析构并释放所有内存块";
}

void MemoryPool::expandPool(size_t poolIndex)
{
    size_t blockSize = poolSizes_[poolIndex];
    // 每次为池分配一个 128KB 的大块
    const size_t numBlocksToAlloc = 128 * 1024 / blockSize;
    try
    {
        char *newBlock = new char[blockSize * numBlocksToAlloc];
        allBlocks_.emplace_back(newBlock);
        for (size_t i = 0; i < numBlocksToAlloc; ++i)
        {
            freeLists_[blockSize].push_back(newBlock + i * blockSize);
        }
    }
    catch (const std::bad_alloc &e)
    {
        DLOG_ERROR << "[MemoryPool] 扩展内存池失败 (大小: " << blockSize << "): " << e.what();
    }
}

void *MemoryPool::allocate(size_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 找到最合适的内存池
    for (size_t i = 0; i < poolSizes_.size(); ++i)
    {
        if (size <= poolSizes_[i])
        {
            if (freeLists_[poolSizes_[i]].empty())
            {
                expandPool(i);
            }
            if (!freeLists_[poolSizes_[i]].empty())
            {
                void *block = freeLists_[poolSizes_[i]].back();
                freeLists_[poolSizes_[i]].pop_back();
                return block;
            }
        }
    }

    // 如果请求的大小超过了最大的池，则直接使用 new
    DLOG_WARN << "[MemoryPool] 分配大小 " << size << " 超过最大池限制，回退到 new";
    return ::operator new(size);
}

void MemoryPool::deallocate(void *ptr, size_t size)
{
    if (!ptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 找到对应的内存池
    for (size_t i = 0; i < poolSizes_.size(); ++i)
    {
        if (size <= poolSizes_[i])
        {
            freeLists_[poolSizes_[i]].push_back(ptr);
            return;
        }
    }

    // 如果是大块内存，则使用 delete
    DLOG_WARN << "[MemoryPool] 释放大小 " << size << " 超过最大池限制，使用 delete";
    ::operator delete(ptr);
}