# 收集所有模块的源文件
file(GLOB_RECURSE BASE_SRC_FILES CONFIGURE_DEPENDS "base/*.cpp")
file(GLOB_RECURSE LOG_SRC_FILES CONFIGURE_DEPENDS "log/*.cpp")
file(GLOB_RECURSE NET_SRC_FILES CONFIGURE_DEPENDS "net/*.cpp")
file(GLOB_RECURSE HTTP_SRC_FILES CONFIGURE_DEPENDS "http/*.cpp")
file(GLOB_RECURSE DB_SRC_FILES CONFIGURE_DEPENDS "db/*.cpp")
file(GLOB_RECURSE SSL_SRC_FILES CONFIGURE_DEPENDS "ssl/*.cpp")
file(GLOB_RECURSE WEBSOCKET_SRC_FILES CONFIGURE_DEPENDS "websocket/*.cpp")
file(GLOB_RECURSE APP_SRC_FILES CONFIGURE_DEPENDS "app/*.cpp")

# 合并所有源文件
set(WEBSERVER_SRC_FILES
    ${BASE_SRC_FILES}
    ${LOG_SRC_FILES}
    ${NET_SRC_FILES}
    ${HTTP_SRC_FILES}
    ${DB_SRC_FILES}
    ${SSL_SRC_FILES}
    ${WEBSOCKET_SRC_FILES}
    ${APP_SRC_FILES}
)

# 创建动态库
add_library(webserver_lib SHARED ${WEBSERVER_SRC_FILES})

# 设置动态库的属性
set_target_properties(webserver_lib PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME webserver
)

# 为动态库添加头文件搜索路径
target_include_directories(webserver_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include/base
        ${CMAKE_SOURCE_DIR}/include/log
        ${CMAKE_SOURCE_DIR}/include/net
        ${CMAKE_SOURCE_DIR}/include/http
        ${CMAKE_SOURCE_DIR}/include/http/handlers
        ${CMAKE_SOURCE_DIR}/include/db
        ${CMAKE_SOURCE_DIR}/include/ssl
        ${CMAKE_SOURCE_DIR}/include/websocket
        ${CMAKE_SOURCE_DIR}/include/app
)

# 查找并链接所需的第三方库
find_package(yaml-cpp REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 链接第三方库到动态库
target_link_libraries(webserver_lib 
    PRIVATE 
        yaml-cpp::yaml-cpp 
        jwt-cpp::jwt-cpp
        mysqlclient
        OpenSSL::SSL 
        OpenSSL::Crypto 
        Threads::Threads
)