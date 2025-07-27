#include "ssl/SSLContext.h"
#include <openssl/err.h>
#include "log/Log.h"

/**
 * @brief OpenSSL全局初始化器
 * @details 确保OpenSSL库在使用前被正确初始化，包括算法库、错误字符串等
 */
struct SSLInitializer
{
    /**
     * @brief 构造函数，执行OpenSSL全局初始化
     * @details 初始化SSL库、加载所有算法、加载错误字符串
     */
    SSLInitializer()
    {
        SSL_library_init();           // 初始化SSL库
        OpenSSL_add_all_algorithms(); // 加载所有加密算法
        SSL_load_error_strings();     // 加载错误字符串
    }
};

// 全局静态初始化器，确保OpenSSL在使用前被初始化
static SSLInitializer ssl_initializer;

/**
 * @brief 构造函数，初始化SSL上下文
 * @param certPath 服务器证书文件路径
 * @param keyPath 服务器私钥文件路径
 * @details 创建SSL_CTX对象，加载证书和私钥，验证私钥与证书的匹配性
 */
SSLContext::SSLContext(const std::string &certPath, const std::string &keyPath)
    : ssl_ctx_(nullptr)
{
    // 创建TLS服务器方法的SSL上下文
    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_)
    {
        DLOG_FATAL << "SSL_CTX_new error";
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 加载服务器证书文件
    if (SSL_CTX_use_certificate_file(ssl_ctx_, certPath.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        DLOG_FATAL << "SSL_CTX_use_certificate_file failed for cert: " << certPath;
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 加载服务器私钥文件
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, keyPath.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        DLOG_FATAL << "SSL_CTX_use_PrivateKey_file failed for key: " << keyPath;
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 验证私钥与证书是否匹配
    if (!SSL_CTX_check_private_key(ssl_ctx_))
    {
        DLOG_FATAL << "Private key does not match the public certificate";
        ERR_print_errors_fp(stderr);
        abort();
    }

    DLOG_INFO << "[SSLContext] SSL 上下文初始化成功";
}

/**
 * @brief 析构函数，释放SSL_CTX
 * @details 自动释放SSL_CTX对象，防止内存泄漏
 */
SSLContext::~SSLContext()
{
    if (ssl_ctx_)
    {
        SSL_CTX_free(ssl_ctx_); // 释放SSL上下文对象
    }
}