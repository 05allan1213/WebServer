#include "Thread.h"

#include <semaphore.h>

#include "CurrentThread.h"
#include "log/Log.h"

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name)
{
    setDefaultName(); // 设置默认线程名
}

Thread::~Thread()
{
    // 如果线程已启动但没有被 join,则将其设置为 detach 状态
    // 这样主线程退出时,该子线程资源会被系统自动回收,避免资源泄漏
    // detach 和 join 是互斥的
    if (started_ && !joined_)
    {
        thread_->detach(); // 设置为分离线程
    }
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    if (sem_init(&sem, 0, 0))
    {
        DLOG_FATAL << "sem_init error";
    }
    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread(
        [&]()
        {
            tid_ = CurrentThread::tid(); // 获取当前线程的tid
            if (sem_post(&sem))          // V操作
            {
                DLOG_FATAL << "sem_post error";
            }
            func_(); // 开启一个新线程,专门执行一个线程函数
        }));
    // 这里必须等待获取上面新创建的线程的tid
    if (sem_wait(&sem)) // P操作
    {
        DLOG_FATAL << "sem_wait error";
    }
}

void Thread::join()
{
    joined_ = true;
    if (started_)
    {
        thread_->join();
    }
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}