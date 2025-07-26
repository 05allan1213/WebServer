# 查找当前目录下的所有 .cpp 源文件
file(GLOB_RECURSE BASE_SRC_FILES "*.cpp")

# 创建一个名为 "base" 的静态库
add_library(base STATIC ${BASE_SRC_FILES})

# 为 base 库添加头文件搜索路径
target_include_directories(base
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/base
)

# 查找并链接所需的第三方库
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(base 
    PRIVATE 
        yaml-cpp::yaml-cpp 
        Threads::Threads
)