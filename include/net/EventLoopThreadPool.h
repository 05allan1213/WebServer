#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/noncopyable.h"

class EventLoop;
class EventLoopThread;

/**
 * @brief EventLoop线程池类,管理多个IO线程
 *
 * EventLoopThreadPool实现了主从Reactor模式中的从Reactor池,负责管理多个IO线程。
 * 主要功能：
 * - 创建和管理多个EventLoopThread
 * - 提供轮询算法分配连接给不同的IO线程
 * - 支持线程池的配置和启动
 * - 提供线程安全的EventLoop访问接口
 *
 * 设计特点：
 * - 采用轮询算法进行负载均衡
 * - 支持线程池的配置参数(队列大小、保活时间等)
 * - 线程安全的启动和访问机制
 * - 与TcpServer配合实现高性能的网络服务
 *
 * 使用场景：
 * - 当TcpServer需要处理大量并发连接时
 * - 需要将连接分散到多个IO线程以提高性能
 * - 实现主从Reactor模式中的从Reactor池
 */
class EventLoopThreadPool : noncopyable
{
public:
    /**
     * @brief 线程初始化回调函数类型
     * @param loop 新创建的EventLoop指针
     *
     * 当每个IO线程的EventLoop创建完成后调用,用于线程特定的初始化
     */
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    /**
     * @brief 构造函数
     * @param baseLoop 主EventLoop指针,通常由用户创建
     * @param nameArg 线程池名称
     * @param epollMode epoll触发模式("ET"/"LT"),默认LT
     *
     * 创建线程池对象,但不立即启动线程
     */
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg, const std::string &epollMode = "LT");

    /**
     * @brief 析构函数
     *
     * 停止所有线程,清理资源
     */
    ~EventLoopThreadPool();

    /**
     * @brief 设置线程池中的线程数量
     * @param numThreads 线程数量,0表示单线程模式
     *
     * 必须在start()之前调用,设置后不能修改
     */
    void setThreadNum(int numThreads);

    /**
     * @brief 设置任务队列大小
     * @param queueSize 队列大小,默认为1000
     */
    void setQueueSize(int queueSize);

    /**
     * @brief 设置线程保活时间
     * @param keepAliveTime 保活时间(秒),默认为60秒
     */
    void setKeepAliveTime(int keepAliveTime);

    /**
     * @brief 设置最大空闲线程数
     * @param maxIdleThreads 最大空闲线程数,默认为5
     */
    void setMaxIdleThreads(int maxIdleThreads);

    /**
     * @brief 设置最小空闲线程数
     * @param minIdleThreads 最小空闲线程数,默认为1
     */
    void setMinIdleThreads(int minIdleThreads);

    /**
     * @brief 获取任务队列大小
     * @return 队列大小
     */
    int getQueueSize() const { return queueSize_; }

    /**
     * @brief 获取线程保活时间
     * @return 保活时间(秒)
     */
    int getKeepAliveTime() const { return keepAliveTime_; }

    /**
     * @brief 获取最大空闲线程数
     * @return 最大空闲线程数
     */
    int getMaxIdleThreads() const { return maxIdleThreads_; }

    /**
     * @brief 获取最小空闲线程数
     * @return 最小空闲线程数
     */
    int getMinIdleThreads() const { return minIdleThreads_; }

    /**
     * @brief 启动线程池
     * @param cb 线程初始化回调函数,默认为空
     *
     * 创建并启动所有IO线程,等待所有EventLoop初始化完成
     */
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    /**
     * @brief 获取下一个EventLoop
     * @return 返回下一个可用的EventLoop指针
     *
     * 采用轮询算法,依次返回线程池中的EventLoop,实现负载均衡
     */
    EventLoop *getNextLoop();

    /**
     * @brief 获取线程池中所有的EventLoop指针
     * @return EventLoop指针数组
     *
     * 返回所有IO线程的EventLoop,包括主EventLoop
     */
    std::vector<EventLoop *> getAllLoops();

    /**
     * @brief 检查线程池是否已启动
     * @return 如果已启动返回true,否则返回false
     */
    bool started() const { return started_; }

    /**
     * @brief 获取线程池名称
     * @return 线程池名称字符串
     */
    const std::string &name() const { return name_; }

private:
    EventLoop *baseLoop_; // 用户创建的主EventLoop,负责接受新连接
    std::string name_;    // 线程池名称,用于标识和日志
    bool started_;        // 线程池是否已启动的标志
    int threadNum_;       // 线程池中IO线程的数量
    int next_;            // 用于getNextLoop()轮询的下一个索引

    // 线程池配置参数
    int queueSize_ = 1000;   // 任务队列大小,默认为1000
    int keepAliveTime_ = 60; // 线程保活时间(秒),默认为60秒
    int maxIdleThreads_ = 5; // 最大空闲线程数,默认为5
    int minIdleThreads_ = 1; // 最小空闲线程数,默认为1
    std::string epollMode_;  // epoll触发模式,"ET"=边缘触发,"LT"=水平触发

    // 线程池管理的线程和EventLoop
    std::vector<std::unique_ptr<EventLoopThread>>
        threads_;                    // 存储EventLoopThread对象的智能指针数组
    std::vector<EventLoop *> loops_; // 存储线程池中所有EventLoop的指针
};