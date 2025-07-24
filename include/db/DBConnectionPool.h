#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>
#include <mysql/mysql.h>
#include <chrono>

#include "log/Log.h"
#include "db/DBConfig.h"

/**
 * @brief 数据库连接对象封装
 * 封装MYSQL*和空闲时间戳，用于连接池管理。
 */
class Connection
{
public:
    /**
     * @brief 构造函数
     * @param conn MySQL连接句柄
     */
    Connection(MYSQL *conn)
        : m_conn(conn), m_aliveTime(std::chrono::steady_clock::now()) {}
    /**
     * @brief 刷新空闲起始时间
     */
    void refreshAliveTime()
    {
        m_aliveTime = std::chrono::steady_clock::now();
    }
    /**
     * @brief 获取空闲时长(秒)
     * @return 空闲秒数
     */
    long long getIdleTime() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - m_aliveTime)
            .count();
    }
    MYSQL *m_conn;                                     // MySQL连接句柄
    std::chrono::steady_clock::time_point m_aliveTime; // 空闲起始时间
};

/**
 * @brief 数据库连接池，线程安全的MySQL连接池单例
 * 支持自动回收、健康检查、自动重连等功能。
 */
class DBConnectionPool
{
public:
    /**
     * @brief 获取连接池单例实例
     * @return 单例指针
     */
    static DBConnectionPool *getInstance();

    // 禁止拷贝和赋值
    DBConnectionPool(const DBConnectionPool &) = delete;
    DBConnectionPool &operator=(const DBConnectionPool &) = delete;

    /**
     * @brief 初始化连接池
     * @param config 数据库配置对象
     */
    void init(const DBConfig &config);

    /**
     * @brief 从连接池获取一个可用连接
     * @return 可用连接指针，失败返回nullptr
     */
    Connection *getConnection();

    /**
     * @brief 归还一个连接到连接池
     * @param conn 连接对象指针
     */
    void releaseConnection(Connection *conn);

    void shutdown();

private:
    /**
     * @brief 构造函数
     */
    DBConnectionPool();
    /**
     * @brief 析构函数，释放所有连接
     */
    ~DBConnectionPool();

    /**
     * @brief 生产新连接的线程函数
     */
    void produceConnectionTask();

    /**
     * @brief 定时扫描并回收空闲连接的线程函数
     */
    void scannerConnectionTask();

private:
    std::string m_host;      // 数据库主机地址
    std::string m_user;      // 数据库用户名
    std::string m_password;  // 数据库密码
    std::string m_dbName;    // 数据库名
    int m_port;              // 数据库端口号
    unsigned int m_initSize; // 连接池初始连接数
    unsigned int m_maxSize;  // 连接池最大连接数
    int m_maxIdleTime;       // 最大空闲时间(秒)
    int m_connectionTimeout; // 获取连接超时时间(毫秒)

    std::queue<Connection *> m_connectionQueue; // 存储空闲连接的队列
    std::mutex m_queueMutex;                    // 保护连接队列的互斥锁
    std::condition_variable m_cond;             // 用于生产者-消费者模型的条件变量

    std::atomic<int> m_connectionCount; // 当前已创建的连接总数
    std::atomic<bool> m_isStop;         // 控制后台线程停止的标志

    std::thread m_producerThread; // 生产新连接的线程
    std::thread m_scannerThread;  // 定时扫描并回收空闲连接的线程

    std::once_flag m_initFlag; // 确保单例实例只被初始化一次
};

/**
 * @brief 数据库连接的RAII封装类
 * 自动获取和释放数据库连接，防止忘记释放导致的连接泄漏。
 */
class ConnectionRAII
{
public:
    /**
     * @brief 构造，自动从池获取连接
     * @param conn [out] 获取到的连接指针
     * @param pool 连接池指针
     */
    ConnectionRAII(Connection **conn, DBConnectionPool *pool)
    {
        if (pool == nullptr)
        {
            return;
        }
        *conn = pool->getConnection();
        m_conn = *conn;
        m_pool = pool;
    }
    /**
     * @brief 析构，自动归还连接
     */
    ~ConnectionRAII()
    {
        if (m_conn != nullptr)
        {
            m_pool->releaseConnection(m_conn);
        }
    }

private:
    Connection *m_conn;       // 持有的连接对象
    DBConnectionPool *m_pool; // 连接池指针
};