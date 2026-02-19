add_library(arena
    src/arena.c
)

target_include_directories(arena
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(arena
    PUBLIC
        logger
        utils
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(arena PRIVATE -Wall)