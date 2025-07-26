#include <gtest/gtest.h>
#include "base/ConfigManager.h"
#include "log/Log.h"

// 定义一个全局测试环境类
class GlobalTestEnvironment : public ::testing::Environment
{
public:
    ~GlobalTestEnvironment() override = default;

    // SetUp() 会在所有测试用例运行之前被调用一次
    void SetUp() override
    {
        initDefaultLogger();
        try
        {
            // 为需要配置的测试加载配置文件
            ConfigManager::getInstance().load("configs/config.yml");
        }
        catch (const std::exception &e)
        {
            DLOG_FATAL << "测试环境配置加载失败: " << e.what();
            exit(EXIT_FAILURE);
        }
    }

    void TearDown() override {}
};

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalTestEnvironment);
    return RUN_ALL_TESTS();
}