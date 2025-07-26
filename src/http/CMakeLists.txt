file(GLOB_RECURSE HTTP_SRC_FILES "*.cpp")

add_library(http STATIC ${HTTP_SRC_FILES})

target_include_directories(http
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/http
)

find_package(OpenSSL REQUIRED)

target_link_libraries(http 
    PRIVATE 
        base 
        log 
        net 
        websocket 
        OpenSSL::SSL 
        OpenSSL::Crypto
)