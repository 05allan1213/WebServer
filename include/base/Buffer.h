#pragma once

#include <string>
#include <stdexcept>
#include <atomic>
#include <vector>
#include <algorithm>

/**
 * @brief 三段式内存缓冲区，使用内存池管理
 *
 * 实现了高效的内存缓冲区，支持读写操作和内存池优化。
 * 使用三段式设计：预留空间、可读数据、可写空间。
 */
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   // 预留空间大小
    static const size_t kInitialSize = 1024; // 初始缓冲区大小

    /**
     * @brief 构造函数
     * @param initialSize 初始缓冲区大小
     */
    explicit Buffer(size_t initialSize = kInitialSize);

    /**
     * @brief 析构函数，释放内存
     */
    ~Buffer();

    // 禁止拷贝
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    // 允许移动
    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) noexcept;

    /**
     * @brief 交换两个缓冲区的内容
     * @param rhs 要交换的缓冲区
     */
    void swap(Buffer &rhs) noexcept;

    /**
     * @brief 获取可读字节数
     * @return 可读字节数
     */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    /**
     * @brief 获取可写字节数
     * @return 可写字节数
     */
    size_t writableBytes() const { return capacity_ - writerIndex_; }

    /**
     * @brief 获取可预留字节数
     * @return 可预留字节数
     */
    size_t prependableBytes() const { return readerIndex_; }

    /**
     * @brief 获取缓冲区总容量
     * @return 缓冲区总容量
     */
    size_t capacity() const { return capacity_; }

    /**
     * @brief 获取可读数据的起始指针
     * @return 可读数据的起始指针
     */
    const char *peek() const { return data_ + readerIndex_; }

    /**
     * @brief 查找CRLF序列
     * @return 找到的CRLF位置，未找到返回nullptr
     */
    const char *findCRLF() const;

    /**
     * @brief 读取数据直到指定位置
     * @param end 结束位置指针
     */
    void retrieveUntil(const char *end);

    /**
     * @brief 读取指定长度的数据
     * @param len 要读取的长度
     */
    void retrieve(size_t len);

    /**
     * @brief 读取所有可读数据
     */
    void retrieveAll();

    /**
     * @brief 读取所有数据并转换为字符串
     * @return 读取的字符串
     */
    std::string retrieveAllAsString();

    /**
     * @brief 读取指定长度数据并转换为字符串
     * @param len 要读取的长度
     * @return 读取的字符串
     */
    std::string retrieveAsString(size_t len);

    /**
     * @brief 确保有足够的可写空间
     * @param len 需要的可写空间长度
     */
    void ensureWritableBytes(size_t len);

    /**
     * @brief 追加数据到缓冲区
     * @param data 要追加的数据
     * @param len 数据长度
     */
    void append(const char *data, size_t len);

    /**
     * @brief 从文件描述符读取数据
     * @param fd 文件描述符
     * @param saveErrno 保存错误码的指针
     * @return 读取的字节数
     */
    ssize_t readFd(int fd, int *saveErrno);

    /**
     * @brief 向文件描述符写入数据
     * @param fd 文件描述符
     * @param saveErrno 保存错误码的指针
     * @return 写入的字节数
     */
    ssize_t writeFd(int fd, int *saveErrno);

    // 监控接口
    /**
     * @brief 获取活跃缓冲区数量
     * @return 活跃缓冲区数量
     */
    static size_t getActiveBuffers() { return activeBuffers; }

    /**
     * @brief 获取内存池使用的内存量
     * @return 内存池使用的内存量
     */
    static size_t getPoolMemory() { return poolMemory; }

    /**
     * @brief 获取堆内存使用量
     * @return 堆内存使用量
     */
    static size_t getHeapMemory() { return heapMemory; }

    /**
     * @brief 获取缓冲区扩容次数
     * @return 扩容次数
     */
    static size_t getResizeCount() { return resizeCount; }

private:
    /**
     * @brief 获取缓冲区起始位置
     * @return 缓冲区起始位置指针
     */
    char *begin() { return data_; }

    /**
     * @brief 获取缓冲区起始位置（const版本）
     * @return 缓冲区起始位置指针
     */
    const char *begin() const { return data_; }

    /**
     * @brief 获取可写区域起始位置
     * @return 可写区域起始位置指针
     */
    char *beginWrite() { return begin() + writerIndex_; }

    /**
     * @brief 获取可写区域起始位置（const版本）
     * @return 可写区域起始位置指针
     */
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 监控用的静态原子变量
    static std::atomic<size_t> activeBuffers; // 活跃缓冲区数量
    static std::atomic<size_t> poolMemory;    // 内存池使用量
    static std::atomic<size_t> heapMemory;    // 堆内存使用量
    static std::atomic<size_t> resizeCount;   // 扩容次数

    char *data_;         // 缓冲区数据指针
    size_t capacity_;    // 缓冲区总容量
    size_t readerIndex_; // 读指针位置
    size_t writerIndex_; // 写指针位置
    bool fromPool_;      // 标记内存是否来自内存池
};