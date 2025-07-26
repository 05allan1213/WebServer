cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(webserver LANGUAGES CXX)

# 基准测试建议使用 Release 模式以获得准确的性能数据
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# --- 编译器设置 ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 输出目录设置 ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# --- 寻找依赖包 ---
find_package(yaml-cpp REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# --- 从系统中查找 Benchmark 库 ---
find_package(benchmark REQUIRED)

# --- 启用测试并寻找 GTest ---
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# --- 编译项目核心库 ---
include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(src)

# --- 主应用程序 ---
add_executable(webserver src/main.cpp)
target_link_libraries(webserver 
    PRIVATE 
    base log net http db ssl websocket
    yaml-cpp::yaml-cpp 
    jwt-cpp::jwt-cpp 
    mysqlclient
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)

# --- 统一的单元测试可执行文件 ---
file(GLOB_RECURSE TEST_SOURCES 
    "src/tests/test_*.cpp"
)
add_executable(webserver_test ${TEST_SOURCES})
target_link_libraries(webserver_test 
    PRIVATE 
    base log net http db ssl websocket
    GTest::gtest_main
    yaml-cpp::yaml-cpp 
    jwt-cpp::jwt-cpp 
    mysqlclient
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)
add_test(NAME WebServerUnitTests COMMAND webserver_test)

# --- 基准测试可执行文件 ---
add_executable(webserver_bench src/tests/benchmark_server.cpp)
target_link_libraries(webserver_bench
    PRIVATE
    base log net http db ssl websocket
    benchmark::benchmark 
    yaml-cpp::yaml-cpp
    jwt-cpp::jwt-cpp
    mysqlclient
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)