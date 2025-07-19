# 获取当前目录下的所有源文件
file(GLOB_RECURSE BASE_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(base
    Buffer.cpp
    Config.cpp
    # 其他base源文件
)
target_link_libraries(base yaml-cpp)
target_include_directories(base PUBLIC ${CMAKE_SOURCE_DIR}/include)