add_library(db_handler
    src/db_handler.c
)

target_include_directories(db_handler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(db_handler
    PUBLIC
        utils
        query_manager
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(db_handler PRIVATE -Wall)