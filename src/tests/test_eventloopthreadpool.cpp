#include <gtest/gtest.h>
#include "net/EventLoopThreadPool.h"
#include "net/EventLoop.h"
#include <memory>
#include <set>

TEST(EventLoopThreadPoolTest, StartAndGetLoop)
{
    EventLoop baseLoop;
    EventLoopThreadPool pool(&baseLoop, "testPool");
    pool.setThreadNum(2);
    pool.start();
    auto *loop1 = pool.getNextLoop();
    auto *loop2 = pool.getNextLoop();
    EXPECT_NE(loop1, nullptr);
    EXPECT_NE(loop2, nullptr);
    EXPECT_NE(loop1, loop2);
}

TEST(EventLoopThreadPoolTest, RoundRobin)
{
    EventLoop baseLoop;
    EventLoopThreadPool pool(&baseLoop, "testPool");
    pool.setThreadNum(2);
    pool.start();
    auto *loop1 = pool.getNextLoop();
    auto *loop2 = pool.getNextLoop();
    auto *loop3 = pool.getNextLoop();
    EXPECT_EQ(loop1, loop3); // 轮询回到第一个
}