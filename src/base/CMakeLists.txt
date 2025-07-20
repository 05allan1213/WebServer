# 获取当前目录下的所有源文件
file(GLOB_RECURSE BASE_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(base
    Buffer.cpp
    BaseConfig.cpp
    # 其他base源文件
)

target_include_directories(base PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 添加 yaml-cpp 依赖
find_package(yaml-cpp REQUIRED)

# 添加依赖
target_link_libraries(base PRIVATE yaml-cpp::yaml-cpp)