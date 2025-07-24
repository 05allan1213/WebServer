#pragma once

#include "Logger.h"
#include "LogAppender.h"
#include "LogFile.h"
#include "base/noncopyable.h"
#include <string>
#include <map>
#include <memory>
#include <mutex>

/**
 * @brief 日志管理器类 - 单例模式
 *
 * LogManager负责管理全局的日志器(Logger)实例,实现了单例模式。
 * 它维护一个Logger实例的映射表,并提供创建、获取Logger的接口。
 * 同时,它实现了日志器的层次化管理,支持从配置文件加载配置。
 * 默认使用异步日志模式,提供监控机制确保日志系统稳定运行。
 */
class LogManager : private noncopyable
{
public:
    /**
     * @brief 获取单例实例
     * @return LogManager单例的引用
     */
    static std::shared_ptr<LogManager> getInstance();

    /**
     * @brief 析构函数,释放资源
     */
    ~LogManager();

    /**
     * @brief 获取指定名称的日志器
     * @param name 日志器名称,默认为"root"
     * @return 日志器智能指针
     * @note 如果指定名称的日志器不存在,将创建一个新的
     */
    Logger::ptr getLogger(const std::string &name = "root");

    /**
     * @brief 获取root Logger
     * @return root Logger的智能指针
     */
    Logger::ptr getRoot() const { return m_root; }

    /**
     * @brief 初始化日志系统
     * @param asyncLogBasename 异步日志文件基础名,为空则不使用异步日志
     * @param asyncLogRollSize 异步日志单文件最大大小,默认10MB
     * @param asyncLogFlushInterval 异步日志刷新间隔(秒),默认1秒
     * @param rollMode 日志滚动模式,默认按大小滚动
     */
    void init(const std::string &asyncLogBasename = "",
              off_t asyncLogRollSize = 10 * 1024 * 1024,
              int asyncLogFlushInterval = 1,
              LogFile::RollMode rollMode = LogFile::RollMode::SIZE);

    /**
     * @brief 检查异步日志系统状态
     * @return true表示异步日志系统正常工作,false表示异常
     */
    bool checkAsyncLoggingStatus() const;

    /**
     * @brief 重新初始化异步日志系统
     * @return true表示重新初始化成功,false表示失败
     */
    bool reinitializeAsyncLogging();

    /**
     * @brief 检查日志系统是否已初始化
     * @return true表示已初始化,false表示未初始化
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief 设置初始化状态 (主要用于引导日志)
     * @param initialized 新的初始化状态
     */
    void setInitialized(bool initialized) { m_initialized = initialized; }

    /**
     * @brief 设置日志滚动模式
     * @param mode 滚动模式
     */
    void setRollMode(LogFile::RollMode mode);

    /**
     * @brief 当配置更新时，重新应用日志相关的配置
     */
    void onConfigUpdate();

    /**
     * @brief 关闭日志系统，特别是停止异步日志线程
     */
    void shutdown();

private:
    /**
     * @brief 私有构造函数,创建默认的root日志器
     */
    LogManager();

private:
    // 互斥锁,保证线程安全
    std::mutex m_mutex;
    // 根日志器,是所有日志器的父级
    Logger::ptr m_root;
    // 日志器映射表,按名称存储所有Logger实例
    std::map<std::string, Logger::ptr> m_loggers;
    // 是否已初始化标志
    bool m_initialized = false;
    // 当前日志滚动模式
    LogFile::RollMode m_rollMode = LogFile::RollMode::SIZE_HOURLY;
    // 当前日志文件基础名
    std::string m_logBasename;
    // 当前日志滚动大小
    off_t m_rollSize = 0;
    // 当前日志刷新间隔
    int m_flushInterval = 0;
    // 单例实例,用于实现单例模式
    static std::shared_ptr<LogManager> s_instance;
    // 是否是引导日志
    bool m_isBootLogger = false;
};