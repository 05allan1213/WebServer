#include <gtest/gtest.h>
#include "db/DBConnectionPool.h"
#include "db/DBConfig.h"
#include "base/ConfigManager.h"
#include <thread>
#include <vector>
#include <atomic>

TEST(DBConnectionPoolTest, SingleThreadGetRelease)
{
    auto dbConfig = ConfigManager::getInstance().getDBConfig();
    ASSERT_NE(dbConfig, nullptr);
    if (!dbConfig->isValid())
        return;

    auto *pool = DBConnectionPool::getInstance();
    pool->init(*dbConfig);
    auto *conn = pool->getConnection();
    EXPECT_NE(conn, nullptr);
    if (conn)
    {
        pool->releaseConnection(conn);
    }
}

TEST(DBConnectionPoolTest, MultiThreadGetRelease)
{
    auto dbConfig = ConfigManager::getInstance().getDBConfig();
    ASSERT_NE(dbConfig, nullptr);
    if (!dbConfig->isValid())
        return;

    auto *pool = DBConnectionPool::getInstance();
    pool->init(*dbConfig);

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
    {
        if (t.joinable())
            t.join();
    }
    EXPECT_EQ(successCount, 10);
}