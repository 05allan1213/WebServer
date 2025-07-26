file(GLOB_RECURSE NET_SRC_FILES "*.cpp")

add_library(net STATIC ${NET_SRC_FILES})

target_include_directories(net
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/net
)

target_link_libraries(net 
    PRIVATE 
        base 
        log 
        ssl 
        websocket 
)