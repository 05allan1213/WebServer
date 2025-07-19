# 获取当前目录下的所有源文件
file(GLOB_RECURSE NET_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(net STATIC ${NET_SRC_FILES})

# 添加头文件搜索路径
target_include_directories(net PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 添加依赖
target_link_libraries(net PRIVATE base log)