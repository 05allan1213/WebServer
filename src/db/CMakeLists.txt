file(GLOB_RECURSE DB_SRC_FILES "*.cpp")

add_library(db STATIC ${DB_SRC_FILES})

target_include_directories(db
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/db
)

target_link_libraries(db 
    PRIVATE 
        base 
        log 
        mysqlclient
)