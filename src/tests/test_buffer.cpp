#include <gtest/gtest.h>
#include "base/Buffer.h"
#include "base/BaseConfig.h"

struct BufferTestEnv : public ::testing::Environment
{
    void SetUp() override
    {
        BaseConfig::getInstance().load("configs/config.yml");
    }
};

::testing::Environment *const buffer_env = ::testing::AddGlobalTestEnvironment(new BufferTestEnv);

TEST(BufferTest, BasicReadWrite)
{
    Buffer buf;
    std::string data = "hello";
    buf.append(data.c_str(), data.size());
    EXPECT_EQ(buf.readableBytes(), data.size());
    EXPECT_EQ(std::string(buf.peek(), buf.readableBytes()), data);
    buf.retrieve(data.size());
    EXPECT_EQ(buf.readableBytes(), 0);
}

TEST(BufferTest, MultipleAppendRetrieve)
{
    Buffer buf;
    buf.append("abc", 3);
    buf.append("def", 3);
    EXPECT_EQ(std::string(buf.peek(), buf.readableBytes()), "abcdef");
    buf.retrieve(3);
    EXPECT_EQ(std::string(buf.peek(), buf.readableBytes()), "def");
    buf.retrieveAll();
    EXPECT_EQ(buf.readableBytes(), 0);
}

TEST(BufferTest, EmptyAppend)
{
    Buffer buf;
    buf.append("", 0);
    EXPECT_EQ(buf.readableBytes(), 0);
}

TEST(BufferTest, RetrieveAsString)
{
    Buffer buf;
    buf.append("123456", 6);
    std::string s = buf.retrieveAsString(3);
    EXPECT_EQ(s, "123");
    EXPECT_EQ(std::string(buf.peek(), buf.readableBytes()), "456");
}

TEST(BufferTest, EnsureWriteableBytes)
{
    Buffer buf;
    size_t oldWritable = buf.writableBytes();
    buf.ensureWriteableBytes(2048);
    EXPECT_GE(buf.writableBytes(), 2048);
}