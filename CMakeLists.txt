# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# 设置项目名称和语言
project(webserver LANGUAGES CXX)

# 默认使用 Debug 模式（除非用户自行设置了）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# --- 编译器设置 ---
# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
# 强制要求支持 C++17 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 关闭编译器特定的扩展,以提高可移植性
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 输出目录设置 ---
# 设置库文件(.a, .lib, .so, .dll)和可执行文件的输出路径
# 将它们放在目录下的 lib 和 bin 子目录中,便于管理
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib) # 静态库 (.a, .lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib) # 共享库 (.so, .dll)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin) # 可执行文件 (.exe, 无后缀)

# --- 设置头文件搜索路径 ---
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/base)
include_directories(${PROJECT_SOURCE_DIR}/include/log)
include_directories(${PROJECT_SOURCE_DIR}/include/net)
include_directories(${PROJECT_SOURCE_DIR}/include/config)
include_directories(${PROJECT_SOURCE_DIR}/include/http)
include_directories(${PROJECT_SOURCE_DIR}/include/db)
include_directories(${PROJECT_SOURCE_DIR}/include/timer)

# 添加子目录
add_subdirectory(src)

# 添加 yaml-cpp 依赖
find_package(yaml-cpp REQUIRED)

# 添加 jwt-cpp 依赖
find_package(jwt-cpp REQUIRED)

# 添加 google-test 依赖
find_package(GTest REQUIRED)

# 添加 OpenSSL 依赖
find_package(OpenSSL REQUIRED)

# --- 测试程序 ---
#add_executable(test_log src/tests/test_log.cpp) # 测试日志模块
#target_link_libraries(test_log PRIVATE yaml-cpp::yaml-cpp base log) # 链接 log 库

#add_executable(test_net src/tests/test_net.cpp)
#target_link_libraries(test_net PRIVATE yaml-cpp::yaml-cpp base log net)

#add_executable(test_config src/tests/test_config.cpp)
#target_link_libraries(test_config PRIVATE yaml-cpp::yaml-cpp base log net)

add_executable(webserver src/tests/main.cpp)
target_link_libraries(webserver PRIVATE yaml-cpp::yaml-cpp jwt-cpp::jwt-cpp mysqlclient OpenSSL::SSL OpenSSL::Crypto base log net http db)

# 添加google test 程序
# add_executable(test_buffer src/tests/test_buffer.cpp src/base/Buffer.cpp src/base/BaseConfig.cpp src/base/Timestamp.cpp src/log/Log.cpp src/log/LogLevel.cpp src/log/Logger.cpp)
# target_link_libraries(test_buffer PRIVATE yaml-cpp::yaml-cpp jwt-cpp::jwt-cpp mysqlclient GTest::gtest_main base log net http db)
# add_test(NAME BufferTest COMMAND test_buffer)

# add_executable(test_logformatter src/tests/test_logformatter.cpp src/log/LogFormatter.cpp src/log/LogEvent.cpp src/log/LogLevel.cpp)
# target_link_libraries(test_logformatter PRIVATE yaml-cpp::yaml-cpp jwt-cpp::jwt-cpp mysqlclient GTest::gtest_main base log net http db)
# add_test(NAME LogFormatterTest COMMAND test_logformatter)

# add_executable(test_eventloopthreadpool src/tests/test_eventloopthreadpool.cpp src/net/EventLoopThreadPool.cpp src/net/EventLoop.cpp src/net/EventLoopThread.cpp src/net/Thread.cpp)
# target_link_libraries(test_eventloopthreadpool PRIVATE yaml-cpp::yaml-cpp jwt-cpp::jwt-cpp mysqlclient GTest::gtest_main base log net http db)
# add_test(NAME EventLoopThreadPoolTest COMMAND test_eventloopthreadpool)

#add_executable(test_dbconnectionpool src/tests/test_dbconnectionpool.cpp src/db/DBConnectionPool.cpp src/db/DBConfig.cpp)
#target_link_libraries(test_dbconnectionpool PRIVATE yaml-cpp::yaml-cpp jwt-cpp::jwt-cpp mysqlclient GTest::gtest_main base log net http db)
#add_test(NAME DBConnectionPoolTest COMMAND test_dbconnectionpool)
