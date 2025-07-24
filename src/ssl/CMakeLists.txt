# 获取当前目录下的所有源文件
file(GLOB_RECURSE SSL_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(ssl STATIC ${SSL_SRC_FILES})

# 添加头文件搜索路径
target_include_directories(ssl PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 添加OpenSSL依赖
find_package(OpenSSL REQUIRED)

# 链接依赖
target_link_libraries(ssl PRIVATE base log OpenSSL::SSL OpenSSL::Crypto)