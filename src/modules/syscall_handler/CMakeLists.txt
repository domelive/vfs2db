add_library(syscall_handler
    src/syscall_handler.c
)

target_include_directories(syscall_handler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(syscall_handler
    PUBLIC
        db_handler
    PRIVATE
        PkgConfig::FUSE3
        PkgConfig::SQLITE3   # Required because it calls sqlite3_close
)

target_compile_options(syscall_handler PRIVATE -Wall)