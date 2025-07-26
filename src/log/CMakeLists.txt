file(GLOB_RECURSE LOG_SRC_FILES "*.cpp")

add_library(log STATIC ${LOG_SRC_FILES})

target_include_directories(log
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/log
)

find_package(yaml-cpp REQUIRED)

target_link_libraries(log 
    PRIVATE 
        base 
        yaml-cpp::yaml-cpp
)