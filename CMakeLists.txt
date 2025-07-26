# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# 设置项目名称和语言
project(webserver LANGUAGES CXX)

# 默认使用 Debug 模式
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
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

# --- 统一的测试可执行文件 ---
# 收集所有在 src/tests/ 目录下的测试源文件
file(GLOB_RECURSE TEST_SOURCES "src/tests/*.cpp")

# 创建一个名为 "webserver_test" 的测试可执行文件
add_executable(webserver_test ${TEST_SOURCES})

# 为测试目标链接所有必要的库
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

# 将测试可执行文件添加到 CTest，这样就可以用 'ctest' 命令运行
add_test(NAME WebServerTests COMMAND webserver_test)