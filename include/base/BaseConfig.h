#pragma once
#include <yaml-cpp/yaml.h>
#include <string>

class BaseConfig
{
public:
    virtual ~BaseConfig() = default;
    virtual void load(const std::string &filename) = 0;

    // Buffer相关配置接口
    int getBufferInitialSize() const;
    int getBufferMaxSize() const;
    int getBufferGrowthFactor() const;

    static BaseConfig &getInstance();

protected:
    YAML::Node config_;
    BaseConfig() = default;
};