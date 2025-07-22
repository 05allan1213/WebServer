#include "base/BaseConfig.h"
#include "log/Log.h"

// 具体的BaseConfig实现类
class BaseConfigImpl : public BaseConfig
{
public:
    BaseConfigImpl() = default;

    void load(const std::string &filename) override
    {
        DLOG_INFO << "BaseConfig: 开始加载配置文件 " << filename;
        config_ = YAML::LoadFile(filename);
        DLOG_INFO << "BaseConfig: 配置文件加载完成";

        // 读取并记录配置值
        int initialSize = config_["base"]["buffer"]["initial_size"].as<int>();
        int maxSize = config_["base"]["buffer"]["max_size"].as<int>();
        int growthFactor = config_["base"]["buffer"]["growth_factor"].as<int>();

        DLOG_INFO << "BaseConfig: 读取到配置 - buffer.initial_size=" << initialSize
                  << ", buffer.max_size=" << maxSize
                  << ", buffer.growth_factor=" << growthFactor;

        // 验证配置
        validateConfig(initialSize, maxSize, growthFactor);
    }

private:
    void validateConfig(int initialSize, int maxSize, int growthFactor)
    {
        DLOG_INFO << "BaseConfig: 开始验证配置...";

        // 验证初始大小
        if (initialSize <= 0)
        {
            DLOG_ERROR << "BaseConfig: 配置验证失败 - buffer.initial_size必须大于0,当前值: " << initialSize;
            throw std::invalid_argument("buffer.initial_size必须大于0");
        }

        // 验证最大大小
        if (maxSize <= 0)
        {
            DLOG_ERROR << "BaseConfig: 配置验证失败 - buffer.max_size必须大于0,当前值: " << maxSize;
            throw std::invalid_argument("buffer.max_size必须大于0");
        }

        // 验证增长因子
        if (growthFactor <= 1)
        {
            DLOG_ERROR << "BaseConfig: 配置验证失败 - buffer.growth_factor必须大于1,当前值: " << growthFactor;
            throw std::invalid_argument("buffer.growth_factor必须大于1");
        }

        // 验证大小关系
        if (initialSize > maxSize)
        {
            DLOG_ERROR << "BaseConfig: 配置验证失败 - buffer.initial_size不能大于buffer.max_size";
            throw std::invalid_argument("buffer.initial_size不能大于buffer.max_size");
        }

        DLOG_INFO << "BaseConfig: 配置验证通过";
    }
};

BaseConfig &BaseConfig::getInstance()
{
    static BaseConfigImpl instance;
    return instance;
}

int BaseConfig::getBufferInitialSize() const
{
    if (!config_["base"]["buffer"]["initial_size"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 base.buffer.initial_size 缺失，使用默认值 1024";
        return 1024;
    }
    return config_["base"]["buffer"]["initial_size"].as<int>();
}
int BaseConfig::getBufferMaxSize() const
{
    if (!config_["base"]["buffer"]["max_size"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 base.buffer.max_size 缺失，使用默认值 65536";
        return 65536;
    }
    return config_["base"]["buffer"]["max_size"].as<int>();
}
int BaseConfig::getBufferGrowthFactor() const
{
    if (!config_["base"]["buffer"]["growth_factor"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 base.buffer.growth_factor 缺失，使用默认值 2";
        return 2;
    }
    return config_["base"]["buffer"]["growth_factor"].as<int>();
}
std::string BaseConfig::getJwtSecret() const
{
    if (!config_["jwt"]["secret"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 jwt.secret 缺失，使用默认值 'default_secret'";
        return "default_secret";
    }
    return config_["jwt"]["secret"].as<std::string>();
}
int BaseConfig::getJwtExpireSeconds() const
{
    if (!config_["jwt"]["expire_seconds"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 jwt.expire_seconds 缺失，使用默认值 86400";
        return 86400;
    }
    return config_["jwt"]["expire_seconds"].as<int>();
}
std::string BaseConfig::getJwtIssuer() const
{
    if (!config_["jwt"]["issuer"])
    {
        DLOG_WARN << "[BaseConfig] 配置项 jwt.issuer 缺失，使用默认值 'webserver'";
        return "webserver";
    }
    return config_["jwt"]["issuer"].as<std::string>();
}