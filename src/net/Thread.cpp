#include "Thread.h"

#include <semaphore.h>

#include "CurrentThread.h"
#include "log/Log.h"

std::atomic_int Thread::numCreated_(0); // 静态原子变量，记录所有Thread对象创建的线程数量

/**
 * @brief 构造函数
 * @param func 线程要执行的函数
 * @param name 线程名称，默认为空字符串
 * @details 创建Thread对象，但不立即启动线程
 */
Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name)
{
    setDefaultName(); // 设置默认线程名
}

/**
 * @brief 析构函数
 * @details 如果线程还在运行，会自动detach等待线程结束，防止资源泄漏
 */
Thread::~Thread()
{
    // 如果线程已启动但没有被join，则将其设置为detach状态
    // 这样主线程退出时，该子线程资源会被系统自动回收，避免资源泄漏
    // detach和join是互斥的
    if (started_ && !joined_)
    {
        thread_->detach(); // 设置为分离线程
    }
}

/**
 * @brief 启动线程
 * @details 创建std::thread对象并开始执行线程函数，使用信号量确保tid获取的同步
 */
void Thread::start()
{
    started_ = true;
    sem_t sem;
    if (sem_init(&sem, 0, 0)) // 初始化信号量为0
    {
        DLOG_FATAL << "sem_init error";
    }
    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread(
        [&]()
        {
            tid_ = CurrentThread::tid(); // 获取当前线程的tid
            if (sem_post(&sem))          // V操作，通知主线程tid已获取
            {
                DLOG_FATAL << "sem_post error";
            }
            func_(); // 开启一个新线程，专门执行一个线程函数
        }));
    // 这里必须等待获取上面新创建的线程的tid
    if (sem_wait(&sem)) // P操作，等待子线程获取tid完成
    {
        DLOG_FATAL << "sem_wait error";
    }
}

/**
 * @brief 等待线程结束
 * @details 调用std::thread::join()等待线程执行完成
 */
void Thread::join()
{
    joined_ = true;
    if (started_)
    {
        thread_->join();
    }
}

/**
 * @brief 设置默认线程名
 * @details 如果用户没有指定线程名，自动生成一个默认名称
 */
void Thread::setDefaultName()
{
    int num = ++numCreated_; // 原子递增，获取线程编号
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num); // 生成默认线程名格式：Thread1, Thread2...
        name_ = buf;
    }
}