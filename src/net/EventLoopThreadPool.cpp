#include "EventLoopThreadPool.h"

#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), threadNum_(0), next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    // 循环创建并启动指定数量的 EventLoopThread
    for (int i = 0; i < threadNum_; ++i)
    {
        // 1. 创建线程名称
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        // 2. 创建 EventLoopThread 对象
        EventLoopThread *t = new EventLoopThread(cb, buf);
        // 3. 将 EventLoopThread 的 unique_ptr 存入 threads_
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // 4. 启动 EventLoopThread 并获取其内部的 EventLoop 指针
        loops_.push_back(t->startLoop());
    }

    if (threadNum_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    // 轮询获取
    if (!loops_.empty())
    {
        // 获取当前 next_ 索引处的 loop
        loop = loops_[next_];
        // next_ 索引向后移动
        ++next_;
        // 如果 next_ 超出范围，则回绕到 0
        if (static_cast<size_t>(next_) >= loops_.size())
        {
            next_ = 0;
        }
    }
    // 返回选中的 loop (可能是 baseLoop_ 或池中的某个 loop)
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    // 如果没有工作线程，返回只包含 baseLoop_ 的 vector
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    // 否则，返回包含所有工作线程 EventLoop 指针的 vector
    else
    {
        return loops_;
    }
}
