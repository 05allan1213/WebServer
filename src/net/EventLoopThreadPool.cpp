#include "EventLoopThreadPool.h"

#include "EventLoopThread.h"
#include "log/Log.h"

/**
 * @brief 构造函数
 * @param baseLoop 基础EventLoop指针
 * @param nameArg 线程池名称
 * @param epollMode epoll模式，支持ET/LT
 * @details 初始化EventLoopThreadPool对象，设置基础EventLoop和线程池名称
 */
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg, const std::string &epollMode)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), threadNum_(0), next_(0), epollMode_(epollMode)
{
    DLOG_INFO << "EventLoopThreadPool 创建 - 名称: " << name_ << ", 基础EventLoop: " << baseLoop_ << ", epollMode: " << epollMode_;
}

/**
 * @brief 析构函数
 * @details 记录线程池析构信息
 */
EventLoopThreadPool::~EventLoopThreadPool()
{
    DLOG_INFO << "EventLoopThreadPool 析构 - 名称: " << name_;
}

/**
 * @brief 设置线程池线程数量
 * @param numThreads 线程数量
 * @details 设置线程池中工作线程的数量
 */
void EventLoopThreadPool::setThreadNum(int numThreads)
{
    DLOG_INFO << "设置线程池线程数: " << numThreads << " (当前: " << threadNum_ << ")";
    threadNum_ = numThreads;
}

/**
 * @brief 设置任务队列大小
 * @param queueSize 队列大小
 * @details 设置线程池任务队列的最大容量
 */
void EventLoopThreadPool::setQueueSize(int queueSize)
{
    DLOG_INFO << "设置线程池队列大小: " << queueSize << " (当前: " << queueSize_ << ")";
    queueSize_ = queueSize;
}

/**
 * @brief 设置线程保活时间
 * @param keepAliveTime 保活时间（秒）
 * @details 设置空闲线程的最大保活时间
 */
void EventLoopThreadPool::setKeepAliveTime(int keepAliveTime)
{
    DLOG_INFO << "设置线程保活时间: " << keepAliveTime << "秒 (当前: " << keepAliveTime_ << ")";
    keepAliveTime_ = keepAliveTime;
}

/**
 * @brief 设置最大空闲线程数
 * @param maxIdleThreads 最大空闲线程数
 * @details 设置线程池中允许的最大空闲线程数量
 */
void EventLoopThreadPool::setMaxIdleThreads(int maxIdleThreads)
{
    DLOG_INFO << "设置最大空闲线程数: " << maxIdleThreads << " (当前: " << maxIdleThreads_ << ")";
    maxIdleThreads_ = maxIdleThreads;
}

/**
 * @brief 设置最小空闲线程数
 * @param minIdleThreads 最小空闲线程数
 * @details 设置线程池中保持的最小空闲线程数量
 */
void EventLoopThreadPool::setMinIdleThreads(int minIdleThreads)
{
    DLOG_INFO << "设置最小空闲线程数: " << minIdleThreads << " (当前: " << minIdleThreads_ << ")";
    minIdleThreads_ = minIdleThreads;
}

/**
 * @brief 启动线程池
 * @param cb 线程初始化回调函数
 * @details 创建并启动指定数量的EventLoopThread，执行初始化回调
 */
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    DLOG_INFO << "启动线程池 - 名称: " << name_ << ", 配置线程数: " << threadNum_;
    DLOG_INFO << "线程池配置详情:";
    DLOG_INFO << "  - 队列大小: " << queueSize_;
    DLOG_INFO << "  - 保活时间: " << keepAliveTime_ << "秒";
    DLOG_INFO << "  - 最大空闲线程: " << maxIdleThreads_;
    DLOG_INFO << "  - 最小空闲线程: " << minIdleThreads_;

    started_ = true;

    // 使用配置的线程数量
    int actualThreadNum = std::min(threadNum_, maxIdleThreads_);
    actualThreadNum = std::max(actualThreadNum, minIdleThreads_);

    DLOG_INFO << "实际启动线程数: " << actualThreadNum << " (配置: " << threadNum_
              << ", 最大空闲: " << maxIdleThreads_ << ", 最小空闲: " << minIdleThreads_ << ")";

    // 循环创建并启动指定数量的 EventLoopThread
    for (int i = 0; i < actualThreadNum; ++i)
    {
        // 1. 创建线程名称
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        DLOG_INFO << "创建线程 " << i << ": " << buf;

        // 2. 创建 EventLoopThread 对象,传递 epollMode
        EventLoopThread *t = new EventLoopThread(cb, buf, epollMode_);
        DLOG_INFO << "EventLoopThread 对象创建成功: " << t;

        // 3. 将 EventLoopThread 的 unique_ptr 存入 threads_
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        DLOG_INFO << "线程 " << i << " 已添加到线程池";

        // 4. 启动 EventLoopThread 并获取其内部的 EventLoop 指针
        EventLoop *loop = t->startLoop();
        loops_.push_back(loop);
        DLOG_INFO << "线程 " << i << " 启动完成,EventLoop: " << loop;
    }

    if (threadNum_ == 0 && cb)
    {
        DLOG_INFO << "线程数为0,使用基础EventLoop执行回调";
        cb(baseLoop_);
    }

    DLOG_INFO << "线程池启动完成 - 总线程数: " << loops_.size()
              << ", 线程池状态: " << (started_ ? "已启动" : "未启动");
}

/**
 * @brief 获取下一个EventLoop
 * @return EventLoop指针，使用轮询算法选择
 * @details 使用轮询算法从线程池中选择下一个EventLoop，如果没有工作线程则返回基础EventLoop
 */
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    // 轮询获取
    if (!loops_.empty())
    {
        // 获取当前 next_ 索引处的 loop
        loop = loops_[next_];
        DLOG_DEBUG << "轮询获取EventLoop: 索引=" << next_ << ", EventLoop=" << loop;

        // next_ 索引向后移动
        ++next_;
        // 如果 next_ 超出范围,则回绕到 0
        if (static_cast<size_t>(next_) >= loops_.size())
        {
            next_ = 0;
            DLOG_DEBUG << "轮询索引回绕到0";
        }
    }
    else
    {
        DLOG_DEBUG << "线程池为空,返回基础EventLoop: " << baseLoop_;
    }

    // 返回选中的 loop (可能是 baseLoop_ 或池中的某个 loop)
    return loop;
}

/**
 * @brief 获取所有EventLoop
 * @return 包含所有EventLoop指针的vector
 * @details 返回线程池中所有工作线程的EventLoop指针，如果没有工作线程则只返回基础EventLoop
 */
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    // 如果没有工作线程,返回只包含 baseLoop_ 的 vector
    if (loops_.empty())
    {
        DLOG_DEBUG << "获取所有EventLoop: 线程池为空,返回基础EventLoop";
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    // 否则,返回包含所有工作线程 EventLoop 指针的 vector
    else
    {
        DLOG_DEBUG << "获取所有EventLoop: 返回 " << loops_.size() << " 个工作线程EventLoop";
        return loops_;
    }
}
