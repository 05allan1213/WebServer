#pragma once

class ConfigManager;

/**
 * @brief 应用启动引导类
 * @details 负责应用启动时的初始化流程，包括数据库连接池和表结构初始化
 *
 * @note 进程生命周期约定：
 *       - 数据库连接池在进程生命周期内只能初始化一次
 *       - 不支持同一进程内多次启动/停止
 *       - 如需重启服务，必须重启整个进程
 */
class Bootstrap
{
public:
    /**
     * @brief 初始化数据库连接池和表结构
     * @param configManager 配置管理器引用
     * @throws std::runtime_error 如果数据库配置无效或初始化失败
     */
    static void initDatabase(ConfigManager &configManager);
};
