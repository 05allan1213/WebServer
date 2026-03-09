#include "http/perf/PerfRepository.h"
#include "db/DBConnectionPool.h"
#include <algorithm>
#include <cstdio>
#include <mysql/mysql.h>
#include <sstream>

MemoryPerfRepository::MemoryPerfRepository() = default;

void MemoryPerfRepository::seed(size_t count)
{
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
    items_.reserve(count);
    nextId_ = 1;
    static const char *categories[] = {"network", "parser", "router", "storage", "frontend"};

    for (size_t i = 0; i < count; ++i)
    {
        PerfItem item;
        item.id = nextId_++;
        item.name = "memory-item-" + std::to_string(i + 1);
        item.category = categories[i % (sizeof(categories) / sizeof(categories[0]))];
        item.score = 65.0 + static_cast<double>((i * 7) % 31);
        item.source = "memory";
        item.createdAt = "memory-seeded";
        items_.push_back(std::move(item));
    }
}

std::vector<PerfItem> MemoryPerfRepository::listItems(size_t limit, std::string *error)
{
    if (error)
    {
        error->clear();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PerfItem> result;
    result.reserve(std::min(limit, items_.size()));
    for (auto it = items_.rbegin(); it != items_.rend() && result.size() < limit; ++it)
    {
        result.push_back(*it);
    }
    return result;
}

bool MemoryPerfRepository::batchInsert(const std::vector<PerfWriteItem> &items, size_t *inserted, std::string *error)
{
    if (error)
    {
        error->clear();
    }
    if (inserted)
    {
        *inserted = 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &input : items)
    {
        PerfItem item;
        item.id = nextId_++;
        item.name = input.name;
        item.category = input.category;
        item.score = input.score;
        item.source = "memory";
        item.createdAt = "runtime-memory";
        items_.push_back(std::move(item));
    }

    if (inserted)
    {
        *inserted = items.size();
    }
    return true;
}

bool MySQLPerfRepository::available() const
{
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    return conn && conn->m_conn;
}

std::vector<PerfItem> MySQLPerfRepository::listItems(size_t limit, std::string *error)
{
    std::vector<PerfItem> items;
    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        if (error)
        {
            *error = "数据库连接不可用";
        }
        return items;
    }

    limit = std::max<size_t>(1, std::min<size_t>(limit, 200));
    std::string sql =
        "SELECT `id`, `name`, `category`, `score`, `source`, DATE_FORMAT(`created_at`, '%Y-%m-%d %H:%i:%s') "
        "FROM `perf_item` ORDER BY `id` DESC LIMIT " +
        std::to_string(limit);

    if (mysql_query(conn->m_conn, sql.c_str()))
    {
        if (error)
        {
            *error = mysql_error(conn->m_conn);
        }
        return items;
    }

    MYSQL_RES *result = mysql_store_result(conn->m_conn);
    if (!result)
    {
        if (error)
        {
            *error = mysql_error(conn->m_conn);
        }
        return items;
    }

    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr)
    {
        PerfItem item;
        item.id = row[0] ? std::stoll(row[0]) : 0;
        item.name = row[1] ? row[1] : "";
        item.category = row[2] ? row[2] : "";
        item.score = row[3] ? std::stod(row[3]) : 0.0;
        item.source = row[4] ? row[4] : "db";
        item.createdAt = row[5] ? row[5] : "";
        items.push_back(std::move(item));
    }
    mysql_free_result(result);

    if (error)
    {
        error->clear();
    }
    return items;
}

bool MySQLPerfRepository::batchInsert(const std::vector<PerfWriteItem> &items, size_t *inserted, std::string *error)
{
    if (inserted)
    {
        *inserted = 0;
    }

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        if (error)
        {
            *error = "数据库连接不可用";
        }
        return false;
    }

    if (mysql_query(conn->m_conn, "START TRANSACTION"))
    {
        if (error)
        {
            *error = mysql_error(conn->m_conn);
        }
        return false;
    }

    bool ok = true;
    for (const auto &input : items)
    {
        char nameEscaped[256];
        char categoryEscaped[128];
        unsigned long nameLength = mysql_real_escape_string(conn->m_conn, nameEscaped, input.name.c_str(), input.name.size());
        unsigned long categoryLength =
            mysql_real_escape_string(conn->m_conn, categoryEscaped, input.category.c_str(), input.category.size());

        char sql[512];
        std::snprintf(sql, sizeof(sql),
                      "INSERT INTO `perf_item` (`name`, `category`, `score`, `source`) "
                      "VALUES('%.*s', '%.*s', %.3f, 'db')",
                      static_cast<int>(nameLength), nameEscaped,
                      static_cast<int>(categoryLength), categoryEscaped,
                      input.score);

        if (mysql_query(conn->m_conn, sql))
        {
            ok = false;
            if (error)
            {
                *error = mysql_error(conn->m_conn);
            }
            break;
        }
        if (inserted)
        {
            (*inserted)++;
        }
    }

    if (ok)
    {
        mysql_query(conn->m_conn, "COMMIT");
        if (error)
        {
            error->clear();
        }
    }
    else
    {
        mysql_query(conn->m_conn, "ROLLBACK");
    }

    return ok;
}
