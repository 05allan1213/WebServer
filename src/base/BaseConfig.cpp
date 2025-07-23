#include "base/BaseConfig.h"
#include "log/Log.h"

BaseConfig::BaseConfig(const YAML::Node &rootNode)
    : rootNode_(rootNode)
{
    DLOG_INFO << "BaseConfig: 开始解析配置...";
    try
    {
        node_ = rootNode_["base"];

        int initialSize = getBufferInitialSize();
        int maxSize = getBufferMaxSize();
        int growthFactor = getBufferGrowthFactor();
        validateConfig(initialSize, maxSize, growthFactor);

        if (getJwtSecret().empty())
        {
            DLOG_WARN << "[BaseConfig] jwt.secret 为空或未配置";
        }
    }
    catch (const std::exception &e)
    {
        DLOG_ERROR << "BaseConfig: 配置解析或验证失败: " << e.what();
        throw;
    }
}

void BaseConfig::validateConfig(int initialSize, int maxSize, int growthFactor)
{
    DLOG_INFO << "BaseConfig: 开始验证 'base' 部分配置...";
    if (initialSize <= 0)
        throw std::invalid_argument("buffer.initial_size必须大于0");
    if (maxSize <= 0)
        throw std::invalid_argument("buffer.max_size必须大于0");
    if (growthFactor <= 1)
        throw std::invalid_argument("buffer.growth_factor必须大于1");
    if (initialSize > maxSize)
        throw std::invalid_argument("buffer.initial_size不能大于buffer.max_size");
    DLOG_INFO << "BaseConfig: 'base' 部分配置验证通过";
}

int BaseConfig::getBufferInitialSize() const
{
    if (node_ && node_["buffer"] && node_["buffer"]["initial_size"])
    {
        return node_["buffer"]["initial_size"].as<int>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 base.buffer.initial_size 缺失，使用默认值 1024";
    return 1024;
}

int BaseConfig::getBufferMaxSize() const
{
    if (node_ && node_["buffer"] && node_["buffer"]["max_size"])
    {
        return node_["buffer"]["max_size"].as<int>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 base.buffer.max_size 缺失，使用默认值 65536";
    return 65536;
}

int BaseConfig::getBufferGrowthFactor() const
{
    if (node_ && node_["buffer"] && node_["buffer"]["growth_factor"])
    {
        return node_["buffer"]["growth_factor"].as<int>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 base.buffer.growth_factor 缺失，使用默认值 2";
    return 2;
}

std::string BaseConfig::getJwtSecret() const
{
    if (rootNode_ && rootNode_["jwt"] && rootNode_["jwt"]["secret"])
    {
        return rootNode_["jwt"]["secret"].as<std::string>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 jwt.secret 缺失，使用默认值 'default_secret'";
    return "default_secret";
}

int BaseConfig::getJwtExpireSeconds() const
{
    if (rootNode_ && rootNode_["jwt"] && rootNode_["jwt"]["expire_seconds"])
    {
        return rootNode_["jwt"]["expire_seconds"].as<int>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 jwt.expire_seconds 缺失，使用默认值 86400";
    return 86400;
}

std::string BaseConfig::getJwtIssuer() const
{
    if (rootNode_ && rootNode_["jwt"] && rootNode_["jwt"]["issuer"])
    {
        return rootNode_["jwt"]["issuer"].as<std::string>();
    }
    DLOG_WARN << "[BaseConfig] 配置项 jwt.issuer 缺失，使用默认值 'webserver'";
    return "webserver";
}