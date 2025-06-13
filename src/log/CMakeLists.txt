# 获取当前目录下的所有源文件
file(GLOB_RECURSE LOG_SRC_FILES "*.cpp")

# 将源文件编译为静态库
add_library(log STATIC ${LOG_SRC_FILES})