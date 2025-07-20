# 获取当前目录下的所有源文件
file(GLOB_RECURSE HTTP_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(http STATIC ${HTTP_SRC_FILES})

# 添加头文件搜索路径
target_include_directories(http PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 添加 yaml-cpp 依赖
find_package(yaml-cpp REQUIRED)

# 添加依赖
target_link_libraries(http PRIVATE base log net yaml-cpp::yaml-cpp)