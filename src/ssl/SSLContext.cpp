#include "ssl/SSLContext.h"
#include <openssl/err.h>
#include "log/Log.h"

// OpenSSL的初始化需要是全局且仅一次的
struct SSLInitializer
{
    SSLInitializer()
    {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
    }
};

static SSLInitializer ssl_initializer;

SSLContext::SSLContext(const std::string &certPath, const std::string &keyPath)
    : ssl_ctx_(nullptr)
{
    // 使用TLS协议
    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_)
    {
        DLOG_FATAL << "SSL_CTX_new error";
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 设置证书
    if (SSL_CTX_use_certificate_file(ssl_ctx_, certPath.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        DLOG_FATAL << "SSL_CTX_use_certificate_file failed for cert: " << certPath;
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 设置私钥
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, keyPath.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        DLOG_FATAL << "SSL_CTX_use_PrivateKey_file failed for key: " << keyPath;
        ERR_print_errors_fp(stderr);
        abort();
    }

    // 验证私钥
    if (!SSL_CTX_check_private_key(ssl_ctx_))
    {
        DLOG_FATAL << "Private key does not match the public certificate";
        ERR_print_errors_fp(stderr);
        abort();
    }

    DLOG_INFO << "[SSLContext] SSL 上下文初始化成功";
}

SSLContext::~SSLContext()
{
    if (ssl_ctx_)
    {
        SSL_CTX_free(ssl_ctx_);
    }
}