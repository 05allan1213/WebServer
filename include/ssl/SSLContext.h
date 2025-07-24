#pragma once

#include <openssl/ssl.h>
#include <string>
#include "base/noncopyable.h"

/**
 * @brief SSL上下文封装类
 *
 * 负责初始化和管理OpenSSL的SSL_CTX对象，包括加载证书和私钥。
 * 这是一个RAII风格的类，确保SSL_CTX在使用后被正确释放。
 */
class SSLContext : private noncopyable
{
public:
    /**
     * @brief 构造函数，初始化SSL上下文
     *
     * @param certPath  服务器证书文件路径
     * @param keyPath   服务器私钥文件路径
     */
    SSLContext(const std::string &certPath, const std::string &keyPath);

    /**
     * @brief 析构函数，释放SSL_CTX
     */
    ~SSLContext();

    /**
     * @brief 获取SSL_CTX指针
     * @return SSL_CTX*
     */
    SSL_CTX *get() const { return ssl_ctx_; }

private:
    SSL_CTX *ssl_ctx_;
};