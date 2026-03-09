#include "db/DBInit.h"
#include "db/DBConnectionPool.h"
#include "log/LogManager.h"

static bool execSQL(MYSQL *mysql, const std::string &sql)
{
    DLOG_INFO << "SQL: " << sql;
    if (mysql_query(mysql, sql.c_str()))
    {
        DLOG_ERROR << "SQL Error: " << mysql_error(mysql);
        return false;
    }
    DLOG_INFO << "SQL Success: " << sql;
    return true;
}

void ensureUserTableSchema()
{
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        DLOG_WARN << "[DB] 无法获取数据库连接，跳过user表结构检查";
        return;
    }

    execSQL(conn->m_conn,
            "CREATE TABLE IF NOT EXISTS `user` ("
            "  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
            "  `username` VARCHAR(64) NOT NULL,"
            "  `password` CHAR(64) NOT NULL,"
            "  `salt` CHAR(32) NOT NULL,"
            "  PRIMARY KEY (`id`),"
            "  UNIQUE KEY `uk_user_username` (`username`)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");

    const char *alterSql = "ALTER TABLE `user` ADD COLUMN `salt` CHAR(32) NOT NULL DEFAULT '' AFTER `password`";
    if (mysql_query(conn->m_conn, alterSql))
    {
        unsigned int err = mysql_errno(conn->m_conn);
        if (err != 1060)
        {
            DLOG_ERROR << "[DB] user表结构迁移失败 errno=" << err << " error=" << mysql_error(conn->m_conn);
        }
    }
}

void ensurePerfTableSchema()
{
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        DLOG_WARN << "[DB] 无法获取数据库连接，跳过 perf_item 表结构检查";
        return;
    }

    execSQL(conn->m_conn,
            "CREATE TABLE IF NOT EXISTS `perf_item` ("
            "  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
            "  `name` VARCHAR(96) NOT NULL,"
            "  `category` VARCHAR(48) NOT NULL,"
            "  `score` DOUBLE NOT NULL DEFAULT 0,"
            "  `source` VARCHAR(24) NOT NULL DEFAULT 'db',"
            "  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "  PRIMARY KEY (`id`),"
            "  KEY `idx_perf_item_category_id` (`category`, `id`)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");
}
