add_library(parser
    src/parser.c
)

target_include_directories(parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(parser
    PUBLIC
        logger
        utils
        arena
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(parser PRIVATE -Wall)