#pragma once

#include "Logger.h"
#include "LogAppender.h"
#include "noncopyable.h"
#include <string>
#include <map>
#include <memory>
#include <mutex>

/**
 * @brief 日志管理器类 - 单例模式
 *
 * LogManager负责管理全局的日志器(Logger)实例，实现了单例模式。
 * 它维护一个Logger实例的映射表，并提供创建、获取Logger的接口。
 * 同时，它实现了日志器的层次化管理，支持从配置文件加载配置。
 */
class LogManager : public noncopyable
{
public:
    /**
     * @brief 获取单例实例
     * @return LogManager单例的引用
     */
    static LogManager &getInstance();

    /**
     * @brief 获取指定名称的日志器
     * @param name 日志器名称，默认为"root"
     * @return 日志器智能指针
     * @note 如果指定名称的日志器不存在，将创建一个新的
     */
    Logger::ptr getLogger(const std::string &name = "root");

    /**
     * @brief 获取root Logger
     * @return root Logger的智能指针
     */
    Logger::ptr getRoot() const { return m_root; }

    /**
     * @brief 初始化日志系统
     * @param asyncLogBasename 异步日志文件基础名，为空则不使用异步日志
     * @param asyncLogRollSize 异步日志单文件最大大小，默认10MB
     * @param asyncLogFlushInterval 异步日志刷新间隔(秒)，默认3秒
     */
    void init(const std::string &asyncLogBasename = "",
              off_t asyncLogRollSize = 10 * 1024 * 1024,
              int asyncLogFlushInterval = 3);

private:
    /**
     * @brief 私有构造函数，创建默认的root日志器
     */
    LogManager();

private:
    /// 互斥锁，保证线程安全
    std::mutex m_mutex;
    /// 根日志器，是所有日志器的父级
    Logger::ptr m_root;
    /// 日志器映射表，按名称存储所有Logger实例
    std::map<std::string, Logger::ptr> m_loggers;
    /// 是否已初始化标志
    bool m_initialized = false;
};

/**
 * @brief 快速获取日志器的全局函数
 * @param name 日志器名称，默认为"root"
 * @return 日志器智能指针
 */
inline Logger::ptr getLogger(const std::string &name = "root")
{
    return LogManager::getInstance().getLogger(name);
}