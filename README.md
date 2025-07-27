# WebServer 项目简介

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
