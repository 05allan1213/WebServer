#pragma once

#include <cstddef>
#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include "base/noncopyable.h"

/**
 * @brief 线程安全的类似Slab的内存管理器
 *
 * 管理多个不同大小的固定内存块池。当请求内存时，
 * 会从最合适的池中分配。用于取代通用的new/delete，
 * 提升性能并杜绝内存碎片。
 */
class MemoryPool : private noncopyable
{
public:
    /**
     * @brief 获取内存池的单例实例
     * @return 内存池实例的引用
     */
    static MemoryPool &getInstance();

    /**
     * @brief 从池中分配一块指定大小的内存
     * @param size 需要分配的内存大小
     * @return 指向内存块的指针，失败则抛出std::bad_alloc
     */
    void *allocate(size_t size);

    /**
     * @brief 将一块内存归还给池
     * @param ptr 要归还的内存块指针
     * @param size 该内存块的大小
     */
    void deallocate(void *ptr, size_t size);

private:
    /**
     * @brief 私有构造函数
     */
    MemoryPool();

    /**
     * @brief 析构函数
     */
    ~MemoryPool();

    /**
     * @brief 为指定大小的池补充新的内存块
     * @param poolIndex 池的索引
     */
    void expandPool(size_t poolIndex);

    std::mutex mutex_;                                // 互斥锁，保护内存池操作
    std::map<size_t, std::vector<void *>> freeLists_; // 存储不同大小的内存池，键是块大小，值是空闲块列表
    std::vector<std::unique_ptr<char[]>> allBlocks_;  // 存储所有分配过的内存页，用于最终释放
    std::vector<size_t> poolSizes_;                   // 预定义的池大小
};