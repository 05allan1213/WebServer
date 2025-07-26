file(GLOB_RECURSE WEBSOCKET_SRC_FILES "*.cpp")

add_library(websocket STATIC ${WEBSOCKET_SRC_FILES})

target_include_directories(websocket
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/websocket
)

find_package(OpenSSL REQUIRED)

target_link_libraries(websocket 
    PRIVATE 
        base 
        log 
        OpenSSL::SSL 
        OpenSSL::Crypto
)