#pragma once

#include <cstddef>
#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include "base/noncopyable.h"

/**
 * @brief 一个线程安全的、类似 Slab 的内存管理器
 *
 * 管理多个不同大小的固定内存块池。
 * 当请求内存时，会从最合适的池中分配。
 * 用于取代通用的 new/delete，提升性能并杜绝内存碎片。
 */
class MemoryPool : private noncopyable
{
public:
    /**
     * @brief 获取内存池的单例实例
     */
    static MemoryPool &getInstance();

    /**
     * @brief 从池中分配一块指定大小的内存
     * @details 会找到能容纳指定大小的最小内存池进行分配
     * @param size 需要分配的内存大小
     * @return 指向内存块的指针，失败则抛出 std::bad_alloc
     */
    void *allocate(size_t size);

    /**
     * @brief 将一块内存归还给池
     * @param ptr 要归还的内存块指针
     * @param size 该内存块的大小
     */
    void deallocate(void *ptr, size_t size);

private:
    MemoryPool();
    ~MemoryPool();

    /**
     * @brief 为指定大小的池补充新的内存块
     * @param poolIndex 池的索引
     */
    void expandPool(size_t poolIndex);

    std::mutex mutex_;
    // 存储不同大小的内存池，键是块大小，值是空闲块列表
    std::map<size_t, std::vector<void *>> freeLists_;
    // 存储所有分配过的内存页，用于最终释放
    std::vector<std::unique_ptr<char[]>> allBlocks_;
    // 预定义的池大小
    std::vector<size_t> poolSizes_;
};