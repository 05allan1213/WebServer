#pragma once

#include <string>
#include <stdexcept>
#include <atomic>
#include <vector>
#include <algorithm>

/**
 * @brief 三段式内存缓冲区 (完全重构，使用内存池)
 */
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize);
    ~Buffer();

    // 禁止拷贝
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    // 允许移动
    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) noexcept;

    void swap(Buffer &rhs) noexcept;

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return capacity_ - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }
    size_t capacity() const { return capacity_; }

    const char *peek() const { return data_ + readerIndex_; }

    void retrieve(size_t len);
    void retrieveAll();
    std::string retrieveAllAsString();
    std::string retrieveAsString(size_t len);

    void ensureWritableBytes(size_t len);
    void append(const char *data, size_t len);

    ssize_t readFd(int fd, int *saveErrno);
    ssize_t writeFd(int fd, int *saveErrno);

    // --- 监控接口 ---
    static size_t getActiveBuffers() { return activeBuffers; }
    static size_t getPoolMemory() { return poolMemory; }
    static size_t getHeapMemory() { return heapMemory; }
    static size_t getResizeCount() { return resizeCount; }

private:
    char *begin() { return data_; }
    const char *begin() const { return data_; }
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // --- 监控用的静态原子变量 ---
    static std::atomic<size_t> activeBuffers;
    static std::atomic<size_t> poolMemory;
    static std::atomic<size_t> heapMemory;
    static std::atomic<size_t> resizeCount;

    char *data_;
    size_t capacity_;
    size_t readerIndex_;
    size_t writerIndex_;
    bool fromPool_; // 标记内存是否来自内存池
};