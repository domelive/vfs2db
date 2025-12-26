add_library(query_manager
    src/query_manager.c
)

target_include_directories(query_manager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(query_manager
    PUBLIC
        utils
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(query_manager PRIVATE -Wall)