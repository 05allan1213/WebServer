#include "EventLoopThreadPool.h"

#include "EventLoopThread.h"
#include "log/Log.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), threadNum_(0), next_(0)
{
    DLOG_INFO << "EventLoopThreadPool 创建 - 名称: " << name_ << ", 基础EventLoop: " << baseLoop_;
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    DLOG_INFO << "EventLoopThreadPool 析构 - 名称: " << name_;
}

// setter方法实现
void EventLoopThreadPool::setThreadNum(int numThreads)
{
    DLOG_INFO << "设置线程池线程数: " << numThreads << " (当前: " << threadNum_ << ")";
    threadNum_ = numThreads;
}

void EventLoopThreadPool::setQueueSize(int queueSize)
{
    DLOG_INFO << "设置线程池队列大小: " << queueSize << " (当前: " << queueSize_ << ")";
    queueSize_ = queueSize;
}

void EventLoopThreadPool::setKeepAliveTime(int keepAliveTime)
{
    DLOG_INFO << "设置线程保活时间: " << keepAliveTime << "秒 (当前: " << keepAliveTime_ << ")";
    keepAliveTime_ = keepAliveTime;
}

void EventLoopThreadPool::setMaxIdleThreads(int maxIdleThreads)
{
    DLOG_INFO << "设置最大空闲线程数: " << maxIdleThreads << " (当前: " << maxIdleThreads_ << ")";
    maxIdleThreads_ = maxIdleThreads;
}

void EventLoopThreadPool::setMinIdleThreads(int minIdleThreads)
{
    DLOG_INFO << "设置最小空闲线程数: " << minIdleThreads << " (当前: " << minIdleThreads_ << ")";
    minIdleThreads_ = minIdleThreads;
}

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

        // 2. 创建 EventLoopThread 对象
        EventLoopThread *t = new EventLoopThread(cb, buf);
        DLOG_INFO << "EventLoopThread 对象创建成功: " << t;

        // 3. 将 EventLoopThread 的 unique_ptr 存入 threads_
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        DLOG_INFO << "线程 " << i << " 已添加到线程池";

        // 4. 启动 EventLoopThread 并获取其内部的 EventLoop 指针
        EventLoop *loop = t->startLoop();
        loops_.push_back(loop);
        DLOG_INFO << "线程 " << i << " 启动完成，EventLoop: " << loop;
    }

    if (threadNum_ == 0 && cb)
    {
        DLOG_INFO << "线程数为0，使用基础EventLoop执行回调";
        cb(baseLoop_);
    }

    DLOG_INFO << "线程池启动完成 - 总线程数: " << loops_.size()
              << ", 线程池状态: " << (started_ ? "已启动" : "未启动");
}

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
        // 如果 next_ 超出范围，则回绕到 0
        if (static_cast<size_t>(next_) >= loops_.size())
        {
            next_ = 0;
            DLOG_DEBUG << "轮询索引回绕到0";
        }
    }
    else
    {
        DLOG_DEBUG << "线程池为空，返回基础EventLoop: " << baseLoop_;
    }

    // 返回选中的 loop (可能是 baseLoop_ 或池中的某个 loop)
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    // 如果没有工作线程，返回只包含 baseLoop_ 的 vector
    if (loops_.empty())
    {
        DLOG_DEBUG << "获取所有EventLoop: 线程池为空，返回基础EventLoop";
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    // 否则，返回包含所有工作线程 EventLoop 指针的 vector
    else
    {
        DLOG_DEBUG << "获取所有EventLoop: 返回 " << loops_.size() << " 个工作线程EventLoop";
        return loops_;
    }
}
