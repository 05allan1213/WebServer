#include <gtest/gtest.h>
#include "db/DBConnectionPool.h"
#include "db/DBConfig.h"
#include <thread>
#include <vector>
#include <atomic>

// 注意：此测试假设已正确初始化数据库配置文件
// 若无真实数据库，可只测试接口调用和多线程安全性

TEST(DBConnectionPoolTest, SingleThreadGetRelease)
{
    DBConfig::getInstance().load("configs/config.yml");
    if (!DBConfig::getInstance().isValid())
        return; // 跳过无效配置
    auto *pool = DBConnectionPool::getInstance();
    pool->init(DBConfig::getInstance());
    auto *conn = pool->getConnection();
    EXPECT_NE(conn, nullptr);
    pool->releaseConnection(conn);
}

TEST(DBConnectionPoolTest, MultiThreadGetRelease)
{
    DBConfig::getInstance().load("configs/config.yml");
    if (!DBConfig::getInstance().isValid())
        return;
    auto *pool = DBConnectionPool::getInstance();
    pool->init(DBConfig::getInstance());
    std::atomic<int> successCount{0};
    auto worker = [&]()
    {
        auto *conn = pool->getConnection();
        if (conn)
        {
            successCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            pool->releaseConnection(conn);
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(worker);
    }
    for (auto &t : threads)
        t.join();
    EXPECT_EQ(successCount, 10);
}