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
    }
};

BaseConfig &BaseConfig::getInstance()
{
    static BaseConfigImpl instance;
    return instance;
}

int BaseConfig::getBufferInitialSize() const
{
    return config_["base"]["buffer"]["initial_size"].as<int>();
}

int BaseConfig::getBufferMaxSize() const
{
    return config_["base"]["buffer"]["max_size"].as<int>();
}

int BaseConfig::getBufferGrowthFactor() const
{
    return config_["base"]["buffer"]["growth_factor"].as<int>();
}