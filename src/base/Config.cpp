#include "base/Config.h"

Config &Config::getInstance()
{
    static Config instance;
    return instance;
}

bool Config::loaded_ = false;

void Config::load(const std::string &filename)
{
    if (!loaded_)
    {
        config_ = YAML::LoadFile(filename);
        loaded_ = true;
    }
}

// 日志配置
std::string Config::getLogBasename() const
{
    return config_["log"]["basename"].as<std::string>();
}
int Config::getLogRollSize() const
{
    return config_["log"]["roll_size"].as<int>();
}
int Config::getLogFlushInterval() const
{
    return config_["log"]["flush_interval"].as<int>();
}
std::string Config::getLogRollMode() const
{
    return config_["log"]["roll_mode"].as<std::string>();
}
bool Config::getLogEnableFile() const
{
    return config_["log"]["enable_file"].as<bool>();
}
std::string Config::getLogFileLevel() const
{
    return config_["log"]["file_level"].as<std::string>();
}
std::string Config::getLogConsoleLevel() const
{
    return config_["log"]["console_level"].as<std::string>();
}

// 网络配置
int Config::getNetworkPort() const
{
    return config_["network"]["port"].as<int>();
}
int Config::getNetworkThreadNum() const
{
    return config_["network"]["thread_num"].as<int>();
}

std::string Config::getNetworkIp() const
{
    return config_["network"]["ip"].as<std::string>();
}
