#include "app/Bootstrap.h"
#include "base/ConfigManager.h"
#include "db/DBConnectionPool.h"
#include "db/DBInit.h"
#include "log/LogManager.h"

void Bootstrap::initDatabase(ConfigManager &configManager)
{
    auto dbConfig = configManager.getDBConfig();
    if (!dbConfig || !dbConfig->isValid())
    {
        throw std::runtime_error("数据库配置无效或缺失");
    }

    DBConnectionPool::getInstance()->init(*dbConfig);
    DLOG_INFO << "[Bootstrap] 数据库连接池初始化完成";

    ensureUserTableSchema();
    DLOG_INFO << "[Bootstrap] 数据库表结构检查完成";
}
