#include "db/DBConnectionPool.h"
#include <iostream>
#include "log/Logger.h"
#include <yaml-cpp/yaml.h>
#include "db/DBConfig.h"

/**
 * @brief 获取连接池单例实例(Meyers' Singleton)
 * @return 连接池单例指针
 */
DBConnectionPool *DBConnectionPool::getInstance()
{
    static DBConnectionPool pool;
    return &pool;
}

/**
 * @brief 构造函数，初始化成员变量
 */
DBConnectionPool::DBConnectionPool() : m_connectionCount(0), m_isStop(false) {}

/**
 * @brief 析构函数，释放所有连接和资源
 */
DBConnectionPool::~DBConnectionPool()
{
    m_isStop = true;
    m_cond.notify_all(); // 唤醒所有等待的线程，以便它们可以退出

    // 等待后台线程结束(如果它们是成员变量)
    // 这里假设它们是 detached 的，或者在 init 后 join

    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_connectionQueue.empty())
    {
        Connection *conn = m_connectionQueue.front();
        m_connectionQueue.pop();
        mysql_close(conn->m_conn);
        delete conn;
    }
}

/**
 * @brief 初始化连接池，创建初始连接并启动后台线程
 * @param config 数据库配置对象
 */
void DBConnectionPool::init(const DBConfig &config)
{
    DLOG_INFO << "[DBPool] init() called";
    if (!config.isValid())
    {
        DLOG_ERROR << "DBConfig invalid: " << config.getErrorMsg();
        throw std::runtime_error("DBConfig invalid: " + config.getErrorMsg());
    }
    m_host = config.getHost();
    m_user = config.getUser();
    m_password = config.getPassword();
    m_dbName = config.getDBName();
    m_port = config.getPort();
    m_initSize = config.getInitSize();
    m_maxSize = config.getMaxSize();
    m_maxIdleTime = config.getMaxIdleTime();
    m_connectionTimeout = config.getConnectionTimeout();

    // 创建初始连接
    for (unsigned int i = 0; i < m_initSize; ++i)
    {
        MYSQL *conn = mysql_init(nullptr);
        if (conn == nullptr)
        {
            DLOG_ERROR << "MySQL init error!";
            continue;
        }
        conn = mysql_real_connect(conn, m_host.c_str(), m_user.c_str(),
                                  m_password.c_str(), m_dbName.c_str(), m_port, nullptr, 0);
        if (conn == nullptr)
        {
            DLOG_ERROR << "MySQL connect error: " << mysql_error(conn);
            continue;
        }
        Connection *connection = new Connection(conn);
        m_connectionQueue.push(connection);
        m_connectionCount++;
    }
    // 启动生产者线程
    std::thread producer(&DBConnectionPool::produceConnectionTask, this);
    producer.detach();
    // 启动扫描线程
    std::thread scanner(&DBConnectionPool::scannerConnectionTask, this);
    scanner.detach();
    DLOG_INFO << "DBConnectionPool initialized successfully.";
}

/**
 * @brief 生产者线程函数，按需创建新连接
 */
void DBConnectionPool::produceConnectionTask()
{
    while (!m_isStop)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        // 等待，直到队列为空并且连接数未达到上限
        m_cond.wait(lock, [this]()
                    { return m_connectionQueue.empty() || m_isStop; });
        if (m_isStop)
            break;
        if (m_connectionCount < m_maxSize)
        {
            MYSQL *conn = mysql_init(nullptr);
            if (conn)
            {
                conn = mysql_real_connect(conn, m_host.c_str(), m_user.c_str(),
                                          m_password.c_str(), m_dbName.c_str(), m_port, nullptr, 0);
                if (conn)
                {
                    Connection *connection = new Connection(conn);
                    m_connectionQueue.push(connection);
                    m_connectionCount++;
                    DLOG_INFO << "New DB connection produced.";
                }
                else
                {
                    DLOG_ERROR << "MySQL connect error: " << mysql_error(conn);
                    mysql_close(conn);
                }
            }
        }
        m_cond.notify_all();
    }
}

/**
 * @brief 从连接池中获取一个可用连接，自动健康检查和重连
 * @return 可用连接指针，失败返回nullptr
 */
Connection *DBConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    while (m_connectionQueue.empty())
    {
        m_cond.notify_all();
        if (m_cond.wait_for(lock, std::chrono::milliseconds(m_connectionTimeout)) == std::cv_status::timeout)
        {
            if (m_connectionQueue.empty())
            {
                DLOG_WARN << "Get DB connection timeout!";
                return nullptr;
            }
        }
        if (m_isStop)
            return nullptr;
    }
    Connection *connection = m_connectionQueue.front();
    m_connectionQueue.pop();
    // 健康检查，失败自动重连一次
    if (mysql_ping(connection->m_conn) != 0)
    {
        DLOG_WARN << "Invalid DB connection, try to reconnect.";
        mysql_close(connection->m_conn);
        connection->m_conn = mysql_init(nullptr);
        if (connection->m_conn && mysql_real_connect(connection->m_conn, m_host.c_str(), m_user.c_str(),
                                                     m_password.c_str(), m_dbName.c_str(), m_port, nullptr, 0))
        {
            connection->refreshAliveTime();
            DLOG_INFO << "Reconnected DB connection.";
            return connection;
        }
        else
        {
            DLOG_ERROR << "Reconnect DB connection failed.";
            delete connection;
            m_connectionCount--;
            return nullptr;
        }
    }
    connection->refreshAliveTime();
    return connection;
}

/**
 * @brief 归还一个连接到连接池
 * @param conn 连接对象指针
 */
void DBConnectionPool::releaseConnection(Connection *conn)
{
    if (conn != nullptr)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        conn->refreshAliveTime();
        m_connectionQueue.push(conn);
        m_cond.notify_one();
    }
}

/**
 * @brief 扫描并回收空闲连接的线程函数
 * 超过最大空闲时间的多余连接会被释放。
 */
void DBConnectionPool::scannerConnectionTask()
{
    while (!m_isStop)
    {
        // 每 m_maxIdleTime 秒扫描一次
        std::this_thread::sleep_for(std::chrono::seconds(m_maxIdleTime));

        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (m_connectionCount > m_initSize && !m_connectionQueue.empty())
        {
            Connection *connection = m_connectionQueue.front();
            if (connection->getIdleTime() >= m_maxIdleTime)
            {
                m_connectionQueue.pop();
                mysql_close(connection->m_conn);
                delete connection;
                m_connectionCount--;
                DLOG_INFO << "Freed an idle DB connection.";
            }
            else
            {
                break;
            }
        }
    }
}