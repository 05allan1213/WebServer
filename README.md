# WebServer 项目简介

**项目定位**：以学习型高性能网络服务器框架为主，覆盖 Reactor、多线程、HTTP/HTTPS、WebSocket、日志、配置等完整工程链路；同时具备基础可部署能力，适合在小型场景进行验证与演示，但仍需根据实际生产需求做安全、稳定性与运维完善。

这是一个基于 C++11/17 的高性能网络服务器框架，采用现代 C++最佳实践，核心采用 Reactor 模式，支持高并发、高可扩展性，适合学习、研究和实际工程应用。

## 主要特性

- 配置驱动的架构设计（YAML配置文件，灵活定制）
- 高性能网络：自研 EventLoop/EPollPoller，支持高并发 TCP/HTTP
- Reactor模式：主事件循环+多线程池，IO与业务解耦，优雅关闭
- 高性能异步日志系统，支持日志分级、滚动、热重载
- 三段式Buffer设计，高效内存管理
- 可扩展路由：RESTful路由与中间件
- 静态/动态资源服务
- 数据库连接池：内置MySQL连接池，多线程安全
- SSL/HTTPS支持
- WebSocket支持
- 单元测试/压力测试：集成GoogleTest/Google Benchmark
- 一键构建脚本：build.sh自动化构建、测试、打包、安装

## 目录结构

```
.
├── include/         # 头文件（base, net, http, log, db, ssl, websocket等模块）
├── src/             # 源码实现
│   ├── base/        # 基础设施
│   ├── net/         # 网络核心
│   ├── http/        # HTTP协议与服务器
│   ├── log/         # 日志系统
│   ├── db/          # 数据库连接池
│   ├── ssl/         # SSL支持
│   ├── websocket/   # WebSocket支持
│   └── tests/       # 单元测试与基准测试
├── configs/         # 配置文件
├── web_static/      # 静态资源
├── bin/             # 可执行文件输出目录
├── lib/             # 动态库输出目录
├── build.sh         # 一键构建脚本
├── CMakeLists.txt   # CMake构建配置
└── README.md        # 项目说明
```

## 依赖安装

```bash
sudo apt update
sudo apt install -y \
    libyaml-cpp-dev \
    libssl-dev \
    libmysqlclient-dev \
    libgtest-dev \
    libbenchmark-dev \
    libcurl4-openssl-dev
```

注意：`jwt-cpp` 是 header-only 库，可能需要手动安装：

```bash
# 安装 jwt-cpp
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
sudo make install
```

## 数据库配置
```bash
CREATE DATABASE IF NOT EXISTS webserver
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE webserver;

CREATE TABLE IF NOT EXISTS `user` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(64) NOT NULL,
  `password` CHAR(64) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## 快速开始
```bash
# 一键构建
./build.sh build

# 运行主程序
./build.sh run

# 运行单元测试
./build.sh run-test

# 运行压力测试
./build.sh run-bench
```

## 功能演示脚本/步骤

> 默认启用 HTTPS，监听 `127.0.0.1:8443`；如需 HTTP，请在 `configs/config.yml` 中关闭 `network.ssl.enable`。

```bash
# 1) 构建与启动
./build.sh build
./build.sh run
```

```bash
# 2) 访问静态页（index.html）
# 浏览器打开：https://127.0.0.1:8443/
# 或使用 curl（自签证书需 -k）
curl -k https://127.0.0.1:8443/
```

```bash
# 3) 调用 /api/login（需数据库中已有用户）
# 若没有用户，可先注册：
curl -k -X POST https://127.0.0.1:8443/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test_user","password":"123456"}'

# 登录获取 JWT
curl -k -X POST https://127.0.0.1:8443/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"test_user","password":"123456"}'
```

```bash
# 4) WebSocket /echo
# 使用 websocat 或 wscat 连接后发送任意文本，即可收到原样回显
websocat -k wss://127.0.0.1:8443/echo
# 输入 hello -> 服务器回 hello（示例）
```
