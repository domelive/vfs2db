add_library(types
    src/db_schema.c
    src/schema.c
    src/primary_key.c
    src/foreign_key.c
    src/attribute.c
)

target_include_directories(types
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(types
    PUBLIC
        logger
        utils
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(types PRIVATE -Wall)