#include "http/handlers/UserHandler.h"
#include "base/ConfigManager.h"
#include "db/DBConnectionPool.h"
#include "log/LogManager.h"
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

static std::string hashPassword(const std::string &password, const std::string &salt)
{
    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                      (const unsigned char *)salt.c_str(), salt.length(),
                      100000, EVP_sha256(), 32, hash);

    std::stringstream ss;
    for (int i = 0; i < 32; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

static std::string generateSalt()
{
    unsigned char salt[16];
    RAND_bytes(salt, 16);

    std::stringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    return ss.str();
}

static void setJsonError(HttpResponse *resp, HttpResponse::HttpStatusCode code, const std::string &message)
{
    resp->setStatusCode(code);
    resp->setContentType("application/json");
    resp->setBody(json({{"status", "error"}, {"message", message}}).dump());
}

static void logStmtError(const std::string &context, MYSQL_STMT *stmt)
{
    if (!stmt)
        return;
    DLOG_ERROR << context << " errno=" << mysql_stmt_errno(stmt) << " error=" << mysql_stmt_error(stmt);
}

void userRegister(const HttpRequest &req, HttpResponse *resp)
{
    DLOG_INFO << "[UserHandler] 用户注册请求";
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        return;
    }

    std::string username, password;
    try
    {
        auto data = json::parse(req.getBody());
        username = data.at("username").get<std::string>();
        password = data.at("password").get<std::string>();
    }
    catch (json::exception &e)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"请求格式错误或缺少字段"})");
        return;
    }

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误，无法连接数据库");
        return;
    }

    std::string salt = generateSalt();
    std::string hashedPassword = hashPassword(password, salt);

    MYSQL_STMT *stmt = mysql_stmt_init(conn->m_conn);
    if (!stmt)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    const char *sql = "INSERT INTO `user`(`username`, `password`, `salt`) VALUES(?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
    {
        logStmtError("[Register] mysql_stmt_prepare失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库表结构或SQL异常）");
        return;
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)username.c_str();
    bind[0].buffer_length = username.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)hashedPassword.c_str();
    bind[1].buffer_length = hashedPassword.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char *)salt.c_str();
    bind[2].buffer_length = salt.length();

    if (mysql_stmt_bind_param(stmt, bind))
    {
        logStmtError("[Register] mysql_stmt_bind_param失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_execute(stmt))
    {
        unsigned int err = mysql_stmt_errno(stmt);
        logStmtError("[Register] mysql_stmt_execute失败", stmt);
        mysql_stmt_close(stmt);
        if (err == 1062)
        {
            setJsonError(resp, HttpResponse::k409Conflict, "用户名已存在");
        }
        else
        {
            setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库执行失败）");
        }
        return;
    }

    mysql_stmt_close(stmt);
    resp->setStatusCode(HttpResponse::k201Created);
    resp->setContentType("application/json");
    resp->setBody(R"({"status":"success", "message":"用户注册成功"})");
}

void userLogin(const HttpRequest &req, HttpResponse *resp)
{
    DLOG_INFO << "[UserHandler] 用户登录请求";
    if (req.getMethod() != HttpRequest::Method::kPost)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        return;
    }

    std::string username, password;
    try
    {
        auto data = json::parse(req.getBody());
        username = data.at("username").get<std::string>();
        password = data.at("password").get<std::string>();
    }
    catch (json::exception &e)
    {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"请求格式错误或缺少字段"})");
        return;
    }

    Connection *conn = nullptr;
    ConnectionRAII connRAII(&conn, DBConnectionPool::getInstance());
    if (!conn || !conn->m_conn)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误，无法连接数据库");
        return;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(conn->m_conn);
    if (!stmt)
    {
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    const char *sql = "SELECT `id`, `password`, `salt` FROM `user` WHERE `username`=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
    {
        logStmtError("[Login] mysql_stmt_prepare失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库表结构或SQL异常）");
        return;
    }

    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = (char *)username.c_str();
    bind_param[0].buffer_length = username.length();

    if (mysql_stmt_bind_param(stmt, bind_param))
    {
        logStmtError("[Login] mysql_stmt_bind_param失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_execute(stmt))
    {
        logStmtError("[Login] mysql_stmt_execute失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（数据库执行失败）");
        return;
    }

    int user_id;
    char db_password[128];
    char db_salt[64];
    unsigned long password_len, salt_len;

    MYSQL_BIND bind_result[3];
    memset(bind_result, 0, sizeof(bind_result));

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = &user_id;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = db_password;
    bind_result[1].buffer_length = sizeof(db_password);
    bind_result[1].length = &password_len;

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = db_salt;
    bind_result[2].buffer_length = sizeof(db_salt);
    bind_result[2].length = &salt_len;

    if (mysql_stmt_bind_result(stmt, bind_result))
    {
        logStmtError("[Login] mysql_stmt_bind_result失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_store_result(stmt))
    {
        logStmtError("[Login] mysql_stmt_store_result失败", stmt);
        mysql_stmt_close(stmt);
        setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误");
        return;
    }

    if (mysql_stmt_fetch(stmt) == 0)
    {
        std::string dbPasswordHash(db_password, password_len);
        std::string salt(db_salt, salt_len);
        std::string hashedPassword = hashPassword(password, salt);

        if (hashedPassword == dbPasswordHash)
        {
            auto baseConfig = ConfigManager::getInstance().getBaseConfig();
            if (!baseConfig)
            {
                mysql_stmt_close(stmt);
                setJsonError(resp, HttpResponse::k500InternalServerError, "服务器内部错误（JWT配置缺失）");
                return;
            }

            std::string secret = baseConfig->getJwtSecret();
            int expire = baseConfig->getJwtExpireSeconds();
            std::string issuer = baseConfig->getJwtIssuer();

            auto token = jwt::create()
                             .set_issuer(issuer)
                             .set_type("JWS")
                             .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
                             .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds(expire))
                             .sign(jwt::algorithm::hs256(secret));

            json resp_json = {{"status", "success"}, {"token", token}};
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("application/json");
            resp->setBody(resp_json.dump());
        }
        else
        {
            resp->setStatusCode(HttpResponse::k401Unauthorized);
            resp->setContentType("application/json");
            resp->setBody(R"({"status":"error", "message":"用户名或密码错误"})");
        }
    }
    else
    {
        resp->setStatusCode(HttpResponse::k401Unauthorized);
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"error", "message":"用户名或密码错误"})");
    }

    mysql_stmt_close(stmt);
}
