#pragma once
#include <string>
#include <yaml-cpp/yaml.h>

class DBConfig
{
public:
    /**
     * @brief 构造函数，加载并校验配置
     * @param configPath 配置文件路径
     */
    DBConfig(const std::string &configPath);

    /**
     * @brief 配置是否有效
     * @return 有效返回true，否则false
     */
    bool isValid() const { return m_valid; }
    /** @brief 获取数据库主机地址 */
    std::string getHost() const { return m_host; }
    /** @brief 获取数据库用户名 */
    std::string getUser() const { return m_user; }
    /** @brief 获取数据库密码 */
    std::string getPassword() const { return m_password; }
    /** @brief 获取数据库名 */
    std::string getDBName() const { return m_dbName; }
    /** @brief 获取数据库端口号 */
    int getPort() const { return m_port; }
    /** @brief 获取连接池初始连接数 */
    unsigned int getInitSize() const { return m_initSize; }
    /** @brief 获取连接池最大连接数 */
    unsigned int getMaxSize() const { return m_maxSize; }
    /** @brief 获取最大空闲时间(秒) */
    int getMaxIdleTime() const { return m_maxIdleTime; }
    /** @brief 获取获取连接超时时间(毫秒) */
    int getConnectionTimeout() const { return m_connectionTimeout; }
    /** @brief 获取错误信息 */
    std::string getErrorMsg() const { return m_errorMsg; }

private:
    /**
     * @brief 加载并校验配置参数
     * @param configPath 配置文件路径
     */
    void loadAndValidate(const std::string &configPath);

    bool m_valid;            // 配置是否有效
    std::string m_errorMsg;  // 错误信息
    std::string m_host;      // 数据库主机地址
    std::string m_user;      // 数据库用户名
    std::string m_password;  // 数据库密码
    std::string m_dbName;    // 数据库名
    int m_port;              // 数据库端口号
    unsigned int m_initSize; // 连接池初始连接数
    unsigned int m_maxSize;  // 连接池最大连接数
    int m_maxIdleTime;       // 最大空闲时间(秒)
    int m_connectionTimeout; // 获取连接超时时间(毫秒)
};