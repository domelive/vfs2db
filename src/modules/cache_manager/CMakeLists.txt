add_library(cache_manager
    src/cache_manager.c
)

target_include_directories(cache_manager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cache_manager
    PUBLIC
        utils
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(cache_manager PRIVATE -Wall)