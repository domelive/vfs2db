add_library(logger STATIC
    src/logger.c
)

target_include_directories(logger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(logger
    PRIVATE
        PkgConfig::FUSE3
        PkgConfig::SQLITE3   # Required because it calls sqlite3_close
)

target_compile_options(logger PRIVATE -Wall)