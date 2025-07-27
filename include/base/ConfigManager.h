#pragma once

#include "base/noncopyable.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <memory>
#include <shared_mutex>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <filesystem>

// 前向声明，解耦头文件依赖
class BaseConfig;
class NetworkConfig;
class LogConfig;
class DBConfig;

/**
 * @brief 配置管理器，负责加载、管理和热重载所有配置
 *
 * ConfigManager是一个单例，作为整个系统配置的唯一入口。
 * 负责一次性加载和解析YAML配置文件，持有所有具体的配置对象，
 * 监控配置文件变化实现热重载，提供线程安全的配置访问接口。
 */
class ConfigManager : private noncopyable
{
public:
    using ConfigUpdateCallback = std::function<void()>;

    /**
     * @brief 获取ConfigManager的单例实例
     * @return ConfigManager的引用
     */
    static ConfigManager &getInstance();

    /**
     * @brief 析构函数，停止监控线程
     */
    ~ConfigManager();

    /**
     * @brief 加载配置文件并启动热重载监控
     * @param filename 配置文件路径
     * @param hotReloadIntervalSeconds 热重载检查间隔（秒），0表示不启用热重载
     */
    void load(const std::string &filename, unsigned int hotReloadIntervalSeconds = 5);

    /**
     * @brief 获取基础配置对象
     * @return BaseConfig的共享指针
     */
    std::shared_ptr<BaseConfig> getBaseConfig() const;

    /**
     * @brief 获取网络配置对象
     * @return NetworkConfig的共享指针
     */
    std::shared_ptr<NetworkConfig> getNetworkConfig() const;

    /**
     * @brief 获取日志配置对象
     * @return LogConfig的共享指针
     */
    std::shared_ptr<LogConfig> getLogConfig() const;

    /**
     * @brief 获取数据库配置对象
     * @return DBConfig的共享指针
     */
    std::shared_ptr<DBConfig> getDBConfig() const;

    /**
     * @brief 注册配置更新后的回调函数
     * @param name 回调的唯一名称，用于后续注销
     * @param callback 回调函数
     */
    void registerUpdateCallback(const std::string &name, ConfigUpdateCallback callback);

    /**
     * @brief 注销配置更新回调
     * @param name 要注销的回调名称
     */
    void unregisterUpdateCallback(const std::string &name);

    /**
     * @brief 关闭配置管理器
     */
    void shutdown();

private:
    /**
     * @brief 私有构造函数，确保单例
     */
    ConfigManager() = default;

    /**
     * @brief 内部加载和解析函数
     * @details 会被 load() 和热重载线程调用。
     * @return bool 是否加载成功
     */
    bool loadInternal();

    /**
     * @brief 热重载监控线程的执行函数
     * @param intervalSeconds 检查文件变化的间隔
     */
    void watchConfigFile(unsigned int intervalSeconds);

    /**
     * @brief 通知所有已注册的回调函数配置已更新
     */
    void notifyUpdate();

private:
    mutable std::shared_mutex mutex_; // 读写锁，用于保护配置对象的线程安全访问

    std::string configFilename_;                    // 配置文件路径
    std::filesystem::file_time_type lastWriteTime_; // 文件最后修改时间，用于热重载

    YAML::Node rootNode_; // 解析后的YAML根节点

    // 各个模块的配置对象
    std::shared_ptr<BaseConfig> baseConfig_;       // 基础配置
    std::shared_ptr<NetworkConfig> networkConfig_; // 网络配置
    std::shared_ptr<LogConfig> logConfig_;         // 日志配置
    std::shared_ptr<DBConfig> dbConfig_;           // 数据库配置

    // 热重载相关
    std::atomic<bool> hotReloading_ = false; // 是否启用热重载
    std::thread watcherThread_;              // 配置文件监控线程

    std::once_flag m_watcherFlag; // 确保单例实例只被初始化一次

    // 监控线程的互斥锁和条件变量
    std::mutex m_watcherMutex;             // 监控线程的互斥锁
    std::condition_variable m_watcherCond; // 监控线程的条件变量

    // 配置更新回调相关
    std::mutex callbackMutex_;                                              // 回调函数互斥锁
    std::unordered_map<std::string, ConfigUpdateCallback> updateCallbacks_; // 回调函数映射
};