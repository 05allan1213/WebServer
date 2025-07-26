file(GLOB_RECURSE SSL_SRC_FILES "*.cpp")

add_library(ssl STATIC ${SSL_SRC_FILES})

target_include_directories(ssl
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/ssl
)

find_package(OpenSSL REQUIRED)

target_link_libraries(ssl 
    PRIVATE 
        base 
        log 
        OpenSSL::SSL 
        OpenSSL::Crypto
)