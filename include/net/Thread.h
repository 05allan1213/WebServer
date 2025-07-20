#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include "base/noncopyable.h"

/**
 * @brief 线程封装类,提供线程的创建和管理
 *
 * Thread类封装了std::thread,提供了更便捷的线程操作接口。
 * 主要功能：
 * - 封装线程的创建、启动和等待
 * - 提供线程状态查询接口
 * - 支持线程名称设置和获取
 * - 统计已创建线程的数量
 *
 * 设计特点：
 * - RAII设计,自动管理线程资源
 * - 不可拷贝,确保线程对象的唯一性
 * - 使用智能指针管理std::thread对象
 * - 提供线程安全的统计功能
 *
 * 使用场景：
 * - 在EventLoopThread中创建IO线程
 * - 需要自定义线程名称时
 * - 需要统计线程数量时
 */
class Thread : noncopyable
{
public:
    /**
     * @brief 线程函数的类型别名
     *
     * 使用std::function包装,支持函数指针、成员函数、lambda表达式等
     */
    using ThreadFunc = std::function<void()>;

    /**
     * @brief 构造函数
     * @param func 线程要执行的函数
     * @param name 线程名称,默认为空字符串
     *
     * 创建Thread对象,但不立即启动线程
     */
    explicit Thread(ThreadFunc func, const std::string &name = std::string());

    /**
     * @brief 析构函数
     *
     * 如果线程还在运行,会自动join等待线程结束
     */
    ~Thread();

    /**
     * @brief 启动线程
     *
     * 创建std::thread对象并开始执行线程函数
     */
    void start();

    /**
     * @brief 等待线程结束
     *
     * 调用std::thread::join()等待线程执行完成
     */
    void join();

    /**
     * @brief 检查线程是否已启动
     * @return 如果线程已启动返回true,否则返回false
     */
    bool started() const { return started_; }

    /**
     * @brief 获取线程ID
     * @return 线程ID(进程内唯一)
     */
    pid_t tid() const { return tid_; }

    /**
     * @brief 获取线程名称
     * @return 线程名称字符串
     */
    const std::string &name() const { return name_; }

    /**
     * @brief 获取已创建线程的总数
     * @return 所有Thread对象创建的线程数量
     *
     * 这是一个静态方法,统计全局的线程数量
     */
    static int numCreated() { return numCreated_; }

private:
    /**
     * @brief 设置默认线程名
     *
     * 如果用户没有指定线程名,自动生成一个默认名称
     */
    void setDefaultName();

private:
    bool started_;                        // 线程是否已启动的标志
    bool joined_;                         // 线程是否已join的标志
    std::shared_ptr<std::thread> thread_; // 线程对象,使用智能指针管理
    pid_t tid_;                           // 线程ID,在start()时获取
    ThreadFunc func_;                     // 线程要执行的函数
    std::string name_;                    // 线程名称,用于标识和日志
    static std::atomic_int numCreated_;   // 记录所有Thread对象创建的线程数量,原子变量保证线程安全
};