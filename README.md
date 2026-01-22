# WebServer - 高性能 C++ Web 服务器框架

**项目定位**：学习型高性能网络服务器框架，覆盖 Reactor 模式、多线程架构、HTTP/HTTPS、WebSocket、异步日志、数据库连接池、配置驱动等完整工程链路。适合学习网络编程、研究服务器架构，同时具备基础可部署能力，可在小型场景进行验证与演示。

基于 C++11/17 标准，采用现代 C++ 最佳实践（RAII、智能指针、移动语义），核心实现 Reactor 事件驱动模型，支持高并发、高可扩展性。

## 目录

- [核心特性](#核心特性)
- [架构设计](#架构设计)
- [目录结构](#目录结构)
- [依赖安装](#依赖安装)
- [数据库配置](#数据库配置)
- [快速开始](#快速开始)
- [功能演示](#功能演示)
- [配置说明](#配置说明)
- [开发指南](#开发指南)
- [性能优化建议](#性能优化建议)
- [注意事项](#注意事项)
- [故障排查](#故障排查)
- [参考资料](#参考资料)

## 核心特性

### 网络架构
- **Reactor 模式**：主从 Reactor 架构，主 Reactor 负责接受连接，从 Reactor 线程池处理 I/O
- **EventLoop**：One Loop Per Thread，每个线程一个事件循环，线程安全的跨线程回调机制
- **EPollPoller**：基于 Linux epoll 的 I/O 多路复用，支持 ET（边缘触发）和 LT（水平触发）模式
- **TimerQueue**：基于 timerfd 的高效定时器管理，支持一次性和周期性定时器
- **TcpConnection**：单个 TCP 连接抽象，管理输入输出缓冲区，支持 SSL、WebSocket、零拷贝 sendfile

### 应用层协议
- **HTTP/HTTPS**：完整的 HTTP/1.1 协议支持，SSL/TLS 加密通信
- **WebSocket**：全双工通信支持，适合实时应用
- **路由系统**：RESTful 风格路由，支持精确匹配、正则匹配、路径参数（如 `/users/:id`）
- **中间件链**：全局和路由级中间件，支持认证、日志、CORS 等横切关注点

### 基础设施
- **异步日志**：双缓冲机制，前端线程写入内存缓冲区（无锁快速路径），后台线程异步刷盘
- **配置管理**：YAML 配置文件驱动，支持日志、网络、数据库、线程池等模块配置
- **Buffer 设计**：三段式内存缓冲区（prepend | readable | writable），高效的 readFd/writeFd 操作
- **数据库连接池**：MySQL 连接池，多线程安全，支持连接复用和超时管理
- **线程池**：业务线程池，用于 CPU 密集型任务，与 I/O 线程解耦

### 工程质量
- **单元测试**：基于 GoogleTest 的完整测试覆盖
- **性能测试**：基于 Google Benchmark 的压力测试和性能基准
- **一键构建**：build.sh 脚本自动化构建、测试、运行、安装

## 架构设计

### 线程模型

```
┌─────────────┐
│  Main Thread │  主线程：运行主 EventLoop，通过 Acceptor 接受新连接
└──────┬──────┘
       │
       ├─→ ┌──────────────┐
       │   │ I/O Thread 1  │  I/O 线程池：每个线程运行一个 EventLoop
       │   └──────────────┘  处理已建立的 TcpConnection 的读写事件
       │
       ├─→ ┌──────────────┐
       │   │ I/O Thread 2  │
       │   └──────────────┘
       │
       ├─→ ┌──────────────┐
       │   │ I/O Thread N  │
       │   └──────────────┘
       │
       ├─→ ┌──────────────┐
       │   │Business Thread│  业务线程池（可选）：处理 CPU 密集型任务
       │   └──────────────┘
       │
       └─→ ┌──────────────┐
           │Logging Thread │  日志线程：异步日志后台线程，负责刷盘
           └───────��──────┘
```

### Reactor 模式核心组件

- **EventLoop**：事件循环，每个线程一个，管理事件循环生命周期
  - `loop()`：主事件循环
  - `runInLoop()` / `queueInLoop()`：线程安全的跨线程回调
  - 使用 eventfd 实现唤醒机制

- **Channel**：事件分发器，封装文件描述符
  - EventLoop 和 Poller 之间的桥梁
  - 管理读/写/关闭/错误回调
  - 使用 `tie()` 配合 weak_ptr 防止悬空指针

- **EPollPoller**：基于 Linux epoll 的 I/O 多路复用
  - 支持边缘触发（ET）和水平触发（LT）模式
  - 返回活跃的 Channel 列表

- **TimerQueue**：定时器管理，集成到 epoll
  - 使用 timerfd 实现高效定时器事件
  - 支持一次性和周期性定时器

- **TcpServer**：主从 Reactor 模式
  - 主 Reactor：通过 Acceptor 接受连接
  - 从 Reactor：EventLoopThreadPool 处理已建立连接
  - 管理 TcpConnection 生命周期

- **TcpConnection**：单个 TCP 连接抽象
  - 管理输入输出缓冲区（Buffer 类）
  - 支持 SSL、WebSocket、零拷贝 sendfile
  - 使用 shared_ptr 和 enable_shared_from_this 管理生命周期

### 关键设计模式

- **RAII**：智能指针管理资源，自动清理
- **回调机制**：连接/消息/写入事件使用 std::function 回调
- **Noncopyable**：不可拷贝基类（[include/base/noncopyable.h](include/base/noncopyable.h)）
- **共享所有权**：TcpConnection 使用 shared_ptr，传递给回调
- **线程安全**：EventLoop 强制 one-loop-per-thread，跨线程通过 runInLoop()

## 目录结构

```
.
├── include/              # 头文件
│   ├── base/            # 基础设施（Buffer, ConfigManager, noncopyable 等）
│   ├── net/             # 网络核心（EventLoop, Channel, EPollPoller, TcpServer 等）
│   ├── http/            # HTTP 协议与服务器（HttpServer, HttpParser, Router, WebServer）
│   ├── log/             # 日志系统（AsyncLogging, LogManager, Logger）
│   ├── db/              # 数据库连接池（ConnectionPool）
│   ├── ssl/             # SSL/TLS 支持
│   ├── websocket/       # WebSocket 支持
│   └── app/             # 应用层（UserService 等业务逻辑）
├── src/                  # 源码实现（目录结构与 include 对应）
│   ├── main.cpp         # 程序入口
│   └── tests/           # 单元测试与性能测试
├── configs/              # 配置文件
│   └── config.example.yml  # 配置示例
├── web_static/           # 静态资源（HTML, CSS, JS）
├── certs/                # SSL 证书目录
├── logs/                 # 日志输出目录
├── bin/                  # 可执行文件输出目录
├── lib/                  # 动态库输出目录
├── build/                # CMake 构建目录
├── build.sh              # 一键构建脚本
├── CMakeLists.txt        # CMake 构建配置
├── CLAUDE.md             # Claude Code 项目指南
└── README.md             # 项目说明
```

### 核心文件说明

- **Reactor 核心**：[include/net/EventLoop.h](include/net/EventLoop.h), [Channel.h](include/net/Channel.h), [EPollPoller.h](include/net/EPollPoller.h)
- **网络层**：[include/net/TcpServer.h](include/net/TcpServer.h), [TcpConnection.h](include/net/TcpConnection.h), [Acceptor.h](include/net/Acceptor.h)
- **HTTP 层**：[include/http/HttpServer.h](include/http/HttpServer.h), [HttpParser.h](include/http/HttpParser.h), [Router.h](include/http/Router.h), [WebServer.h](include/http/WebServer.h)
- **日志系统**：[include/log/AsyncLogging.h](include/log/AsyncLogging.h), [LogManager.h](include/log/LogManager.h), [Logger.h](include/log/Logger.h)
- **基础设施**：[include/base/Buffer.h](include/base/Buffer.h), [ConfigManager.h](include/base/ConfigManager.h)
- **程序入口**：[src/main.cpp](src/main.cpp)

## 依赖安装

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libyaml-cpp-dev \
    libssl-dev \
    libmysqlclient-dev \
    libgtest-dev \
    libbenchmark-dev \
    libcurl4-openssl-dev
```

### jwt-cpp（Header-Only 库）

```bash
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
sudo make install
```

## 数据库配置

创建数据库和用户表：

```sql
CREATE DATABASE IF NOT EXISTS webserver
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE webserver;

CREATE TABLE IF NOT EXISTS `user` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(64) NOT NULL,
  `password` CHAR(64) NOT NULL,
  `salt` CHAR(32) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

如果已有旧表缺少 `salt` 列，执行迁移：

```sql
ALTER TABLE `user` ADD COLUMN `salt` CHAR(32) NOT NULL DEFAULT '' AFTER `password`;
```

## 快速开始

### 1. 配置文件准备

```bash
# 复制配置示例
cp configs/config.example.yml configs/config.yml

# 编辑配置文件，填写数据库密码、JWT 密钥等
vim configs/config.yml
```

配置文件关键项：
- `database.user`、`database.password`、`database.dbname`：数据库连接信息
- `jwt.secret`：JWT 签名密钥（强随机字符串）
- `network.ssl.enable`：是否启用 HTTPS（默认 true）
- `network.port`：监听端口（默认 8443）

### 2. SSL 证书生成（HTTPS 模式）

```bash
mkdir -p certs
openssl req -x509 -newkey rsa:4096 \
  -keyout certs/server.key \
  -out certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

如需使用 HTTP，在 `configs/config.yml` 中设置 `network.ssl.enable: false`。

### 3. 构建与运行

```bash
# 清理并构建（RelWithDebInfo 模式）
./build.sh clean build

# 运行主程序
./build.sh run

# 运行单元测试
./build.sh run-test

# 运行性能测试
./build.sh run-bench

# 安装到系统（需要 sudo）
sudo ./build.sh install
```

### 4. 直接使用 CMake 构建

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)

# 运行
cd ..
export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
./bin/webserver --config=configs/config.yml
```

## 功能演示

默认监听 `127.0.0.1:8443`（HTTPS）。

### 1. 访问静态页面

```bash
# 浏览器访问
https://127.0.0.1:8443/

# 或使用 curl（自签证书需 -k）
curl -k https://127.0.0.1:8443/
```

### 2. 用户注册

```bash
curl -k -X POST https://127.0.0.1:8443/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test_user","password":"123456"}'
```

### 3. 用户登录（获取 JWT）

```bash
curl -k -X POST https://127.0.0.1:8443/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"test_user","password":"123456"}'
```

返回示例：
```json
{
  "code": 0,
  "message": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }
}
```

### 4. WebSocket 连接

```bash
# 使用 websocat 或 wscat
websocat -k wss://127.0.0.1:8443/echo

# 输入任意文本，服务器会原样回显
```

### 5. 查看日志

```bash
tail -f logs/server.log
```

## 配置说明

配置文件 `configs/config.yml` 包含以下主要配置项：

### 基础配置（base）
- `buffer.initial_size`：缓冲区初始大小（字节）
- `buffer.max_size`：缓冲区最大大小（字节）
- `buffer.growth_factor`：缓冲区扩展倍数

### 日志配置（log）
- `basename`：日志文件基础名（如 `logs/server`）
- `roll_size`：日志滚动大小（字节）
- `flush_interval`：日志刷新间隔（秒）
- `roll_mode`：日志滚动模式（`SIZE_HOURLY` 等）
- `enable_async`：是否启用异步日志（`true`/`false`）
- `file_level`：文件日志级别（`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`）
- `console_level`：控制台日志级别

### 网络配置（network）
- `ip`：监听 IP 地址
- `port`：监听端口
- `epoll_mode`：epoll 触发模式（`ET` 边缘触发 / `LT` 水平触发）
- `idle_timeout`：空闲连接超时时间（秒）
- `ssl.enable`：是否启用 HTTPS
- `ssl.cert_path`：SSL 证书路径
- `ssl.key_path`：SSL 私钥路径
- `thread_pool.thread_num`：I/O 线程池线程数

### 数据库配置（database）
- `host`、`user`、`password`、`dbname`、`port`：MySQL 连接信息
- `initSize`：连接池初始连接数
- `maxSize`：连接池最大连接数
- `maxIdleTime`：连接最大空闲时间（秒）
- `connectionTimeout`：获取连接超时时间（毫秒）

### JWT 配置（jwt）
- `secret`：JWT 签名密钥（强随机字符串）
- `expire_seconds`：Token 有效期（秒）
- `issuer`：JWT 签发者标识

## 开发指南

### 添加新路由

在 [src/main.cpp](src/main.cpp) 或 [src/http/WebServer.cpp](src/http/WebServer.cpp) 中注册路由：

```cpp
// 精确匹配
router->addRoute("GET", "/api/users", [](const HttpRequest& req, HttpResponse& resp) {
    resp.setStatusCode(HttpResponse::k200Ok);
    resp.setBody("{\"users\": []}");
});

// 路径参数
router->addRoute("GET", "/api/users/:id", [](const HttpRequest& req, HttpResponse& resp) {
    std::string userId = req.getPathParam("id");
    // 处理逻辑...
});

// 正则匹配
router->addRoute("GET", R"(/api/posts/(\d+))", [](const HttpRequest& req, HttpResponse& resp) {
    // 处理逻辑...
});
```

### 使用中间件

```cpp
// 全局中间件
router->use([](const HttpRequest& req, HttpResponse& resp,
               const Router::NextCallback& next) {
    LOG_INFO << "Request: " << req.method() << " " << req.path();
    next();  // 调用下一个中间件或路由处理器
});

// 路由级中间件
router->addRoute("POST", "/api/admin", adminHandler, {authMiddleware, logMiddleware});
```

### 跨线程操作

网络代码必须在正确的 EventLoop 线程中执行：

```cpp
// 从其他线程调用 TcpConnection 操作
conn->getLoop()->runInLoop([conn]() {
    conn->send("Hello from another thread");
});
```

### 日志使用

```cpp
#include "log/Logger.h"

LOG_DEBUG << "Debug message";
LOG_INFO << "Info message";
LOG_WARN << "Warning message";
LOG_ERROR << "Error message";
LOG_FATAL << "Fatal error";  // 会终止程序
```

## 性能优化建议

1. **I/O 线程数**：设置为 CPU 核心数，避免过多线程切换
2. **epoll 模式**：ET 模式性能更高，但需要正确处理 EAGAIN
3. **异步日志**：生产环境建议启用异步日志，减少 I/O 阻塞
4. **连接池大小**：根据并发量调整数据库连接池大小
5. **Buffer 大小**：根据业务调整初始大小和最大大小

## 注意事项

### 线程安全
- TcpConnection 回调可能在连接关闭后调用，需检查连接状态
- EventLoop 强制 one-loop-per-thread，跨线程操作使用 `runInLoop()`
- 异步日志是线程安全的，无需手动加锁

### 资源管理
- TcpConnection 使用 shared_ptr 管理生命周期
- 回调中捕获 shared_ptr 可延长对象生命周期
- 使用 weak_ptr 避免循环引用

### 安全性
- 生产环境必须修改 JWT 密钥为强随机字符串
- 使用正式 SSL 证书替换自签名证书
- 数据库密码不要硬编码在代码中
- 注意防范 SQL 注入、XSS 等常见漏洞

## 故障排查

### 编译错误
- 检查依赖库是否安装完整
- 确认 CMake 版本 >= 3.10
- 查看 [CLAUDE.md](CLAUDE.md) 了解依赖安装方法

### 运行时错误
- 检查配置文件路径和格式
- 确认数据库连接信息正确
- 查看日志文件 `logs/server.log`
- 使用 `ldd bin/webserver` 检查动态库依赖

### 性能问题
- 使用 `./build.sh run-bench` 运行性能测试
- 检查 I/O 线程数配置
- 分析日志查找瓶颈
- 使用 perf、valgrind 等工具分析

## 许可证

本项目仅供学习和研究使用。

## 贡献

欢迎提交 Issue 和 Pull Request。

## 参考资料

- [muduo 网络库](https://github.com/chenshuo/muduo)
- [libevent](https://libevent.org/)
- [Reactor 模式](https://en.wikipedia.org/wiki/Reactor_pattern)
- [epoll 手册](https://man7.org/linux/man-pages/man7/epoll.7.html)
