#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include "base/BaseConfig.h"

/**
 * @brief 三段式内存缓冲区
 *
 * 内存布局:
 * +-------------------+------------------+------------------+
 * | prependable bytes |  readable bytes  |  writable bytes  |
 * |    (前置预留区)    |   (可读数据区)    |   (可写数据区)    |
 * +-------------------+------------------+------------------+
 * |                   |                  |                  |
 * 0      <=      readerIndex   <=   writerIndex    <=     size
 *
 * 高效的内存缓冲区实现,借鉴muduo库设计,专为网络I/O和日志系统优化
 */
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   // 前置预留区,大小8字节
    static const size_t kInitialSize = 1024; // 缓冲区(readable + writable)的初始大小,大小1024字节

    /**
     * @brief 构造函数,创建指定初始大小的缓冲区
     * @param initialSize 缓冲区初始大小,默认从配置文件读取
     */
    explicit Buffer(size_t initialSize = 0)
    {
        if (initialSize == 0)
        {
            // 从配置文件读取初始大小
            const auto &baseConfig = BaseConfig::getInstance();
            initialSize = baseConfig.getBufferInitialSize();
        }

        buffer_.resize(kCheapPrepend + initialSize); // 缓冲区总大小 = 预留区 + 初始大小
        readerIndex_ = kCheapPrepend;                // 读索引指向预留区之后
        writerIndex_ = kCheapPrepend;                // 写索引初始时与读索引相同
    }

    // 缓冲区状态查询
    /**
     * @brief 返回可读数据的长度
     * @return 可读数据字节数
     */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    /**
     * @brief 返回可写空间的长度
     * @return 可写入的字节数
     */
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }

    /**
     * @brief 返回前置预留区的长度
     * @return 前置预留区字节数
     */
    size_t prependableBytes() const { return readerIndex_; }

    // 数据访问
    /**
     * @brief 返回可读数据的起始地址
     * @return 可读数据的指针
     */
    const char *peek() const { return begin() + readerIndex_; }

    /**
     * @brief 返回可写区域的起始地址
     * @return 可写区域的指针
     */
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 读操作
    /**
     * @brief 从缓冲区读取指定长度的数据
     * @param len 要读取的字节数
     */
    void retrieve(size_t len)
    {
        if (len < readableBytes()) // 只读取了一部分可读数据
        {
            readerIndex_ += len;
        }
        else // 所有可读数据都被读取了
        {
            retrieveAll();
        }
    }

    /**
     * @brief 读完缓冲区所有数据,并执行复位操作
     */
    void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }

    /**
     * @brief 读取全部可读数据并作为字符串返回
     * @return 所有可读数据的字符串
     */
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

    /**
     * @brief 从缓冲区读取指定长度的数据作为字符串返回
     * @param len 要读取的字节数
     * @return 读取的数据转为字符串
     */
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    // 写操作
    /**
     * @brief 确保缓冲区至少有len字节的可写空间
     * @param len 需要的最小可写空间字节数
     */
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
    }

    /**
     * @brief 向缓冲区写入数据
     * @param data 数据指针
     * @param len 数据长度
     */
    void append(const char *data, size_t len)
    {
        // 1. 确保空间
        ensureWriteableBytes(len);
        // 2. 拷贝数据
        std::copy(data, data + len, beginWrite());
        // 3. 更新写索引
        writerIndex_ += len;
    }

    /**
     * @brief 从文件描述符读取数据到缓冲区
     * @param fd 文件描述符
     * @param saveErrno 用于保存errno的指针
     * @return 读取的字节数,-1表示出错
     */
    ssize_t readFd(int fd, int *saveErrno);

    /**
     * @brief 向文件描述符写入缓冲区数据
     * @param fd 文件描述符
     * @param saveErrno 用于保存errno的指针
     * @return 写入的字节数,-1表示出错
     */
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // 返回底层vector存储区的起始地址
    char *begin()
    {
        return &*buffer_.begin(); // vector底层数组首元素的地址
    }
    const char *begin() const { return &*buffer_.begin(); }

    /**
     * @brief 扩容逻辑,确保有足够的可写空间
     * @param len 需要的最小可写空间字节数
     *
     * 两种策略:
     * 1. 空间复用: 将已有数据向前移动,回收前置空间
     * 2. 内存重分配: 当空间复用也不够时,直接扩大底层vector
     */
    void makeSpace(size_t len)
    {
        // 策略一：空间复用(回收前置区)
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            // 策略二：内存重新分配(当空间复用也不够时)
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();      // 计算需要移动的数据量
            std::copy(begin() + readerIndex_,       // 源：当前可读数据的开始
                      begin() + writerIndex_,       // 源：当前可读数据的结束
                      begin() + kCheapPrepend);     // 目标：标准前置空间之后的位置
            readerIndex_ = kCheapPrepend;           // 更新读索引到移动后数据的新起始位置
            writerIndex_ = readerIndex_ + readable; // 更新写索引到移动后数据的新结束位置
        }
    }

    std::vector<char> buffer_; // 缓冲区
    size_t readerIndex_;       // 读索引,应用程序从这里开始读取数据
    size_t writerIndex_;       // 写索引,新数据从这里开始写入
};