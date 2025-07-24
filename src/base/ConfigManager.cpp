#include "base/ConfigManager.h"
#include "base/BaseConfig.h"
#include "log/LogConfig.h"
#include "net/NetworkConfig.h"
#include "db/DBConfig.h"
#include "log/Log.h"
#include <filesystem>
#include <iostream>
#include <chrono>

ConfigManager &ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::~ConfigManager()
{
    if (hotReloading_)
    {
        shutdown();
    }
}

void ConfigManager::load(const std::string &filename, unsigned int hotReloadIntervalSeconds)
{
    configFilename_ = filename;
    if (loadInternal())
    {
        DLOG_INFO << "[ConfigManager] 配置文件 '" << filename << "' 加载成功";
    }
    else
    {
        DLOG_ERROR << "[ConfigManager] 配置文件 '" << filename << "' 加载失败";
    }

    if (hotReloadIntervalSeconds > 0)
    {
        // 使用 call_once 来确保监控线程只被启动一次
        std::call_once(m_watcherFlag, [&]()
                       {
            hotReloading_ = true;
            watcherThread_ = std::thread(&ConfigManager::watchConfigFile, this, hotReloadIntervalSeconds);
            DLOG_INFO << "[ConfigManager] 启动热重载监控, 间隔: " << hotReloadIntervalSeconds << "s"; });
    }
}

bool ConfigManager::loadInternal()
{
    try
    {
        // 加载并解析YAML文件
        YAML::Node newRootNode = YAML::LoadFile(configFilename_);
        lastWriteTime_ = std::filesystem::last_write_time(configFilename_);

        // 创建新的配置对象
        auto newBaseConfig = std::make_shared<BaseConfig>(newRootNode); // BaseConfig 现在接收整个根节点
        auto newNetworkConfig = std::make_shared<NetworkConfig>(newRootNode["network"]);
        auto newLogConfig = std::make_shared<LogConfig>(newRootNode["log"]);
        auto newDBConfig = std::make_shared<DBConfig>(newRootNode["database"]);

        // 使用写锁来原子地更新所有配置对象
        std::unique_lock<std::shared_mutex> lock(mutex_);
        rootNode_ = newRootNode;
        baseConfig_ = newBaseConfig;
        networkConfig_ = newNetworkConfig;
        logConfig_ = newLogConfig;
        dbConfig_ = newDBConfig;

        return true;
    }
    catch (const std::exception &e)
    {
        // 使用 DLOG 和 cerr 双重保障，确保错误信息一定能被看到
        std::string errMsg = "[ConfigManager] 解析配置文件 '" + configFilename_ + "' 失败: " + e.what();
        DLOG_ERROR << errMsg;
        std::cerr << errMsg << std::endl;
        return false;
    }
}

void ConfigManager::watchConfigFile(unsigned int intervalSeconds)
{
    while (hotReloading_)
    {
        std::unique_lock<std::mutex> lock(m_watcherMutex);
        // 它会等待 intervalSeconds 秒，或者被提前唤醒
        m_watcherCond.wait_for(lock, std::chrono::seconds(intervalSeconds));

        if (!hotReloading_) // 检查是否是被 shutdown 唤醒的
        {
            break;
        }

        // 如果是超时唤醒，则执行文件检查逻辑
        try
        {
            if (!std::filesystem::exists(configFilename_))
            {
                continue;
            }

            auto currentWriteTime = std::filesystem::last_write_time(configFilename_);
            if (currentWriteTime != lastWriteTime_)
            {
                DLOG_INFO << "[ConfigManager] 检测到配置文件 '" << configFilename_ << "' 已更新, 准备热重载...";
                if (loadInternal())
                {
                    DLOG_INFO << "[ConfigManager] 配置热重载成功!";
                    notifyUpdate(); // 通知所有订阅者
                }
                else
                {
                    DLOG_ERROR << "[ConfigManager] 配置热重载失败, 继续使用旧配置";
                }
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            DLOG_ERROR << "[ConfigManager] 监控配置文件时出错: " << e.what();
        }
    }
}

void ConfigManager::registerUpdateCallback(const std::string &name, ConfigUpdateCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    updateCallbacks_[name] = callback;
}

void ConfigManager::unregisterUpdateCallback(const std::string &name)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    updateCallbacks_.erase(name);
}

void ConfigManager::shutdown()
{
    hotReloading_ = false;
    m_watcherCond.notify_all();
    if (watcherThread_.joinable())
    {
        watcherThread_.join();
        std::cout << "[ConfigManager] 监控线程已停止。" << std::endl;
    }
}

void ConfigManager::notifyUpdate()
{
    std::vector<ConfigUpdateCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        for (const auto &pair : updateCallbacks_)
        {
            callbacks.push_back(pair.second);
        }
    }

    for (const auto &cb : callbacks)
    {
        if (cb)
        {
            cb(); // 执行回调
        }
    }
}

std::shared_ptr<BaseConfig> ConfigManager::getBaseConfig() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return baseConfig_;
}

std::shared_ptr<NetworkConfig> ConfigManager::getNetworkConfig() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return networkConfig_;
}

std::shared_ptr<LogConfig> ConfigManager::getLogConfig() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return logConfig_;
}

std::shared_ptr<DBConfig> ConfigManager::getDBConfig() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return dbConfig_;
}