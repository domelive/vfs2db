cmake_minimum_required(VERSION 3.15)

# Add module subdirectories
add_subdirectory(modules/utils)
add_subdirectory(modules/query_manager)
add_subdirectory(modules/db_handler)
add_subdirectory(modules/syscall_handler)
add_subdirectory(modules/logger)
add_subdirectory(modules/arena)
add_subdirectory(modules/types)
add_subdirectory(modules/parser)

# Create main executable
add_executable(vfs2db main.c)

# Link against components and external libraries
target_link_libraries(vfs2db
    PRIVATE
        logger
        arena
        syscall_handler
        db_handler
        query_manager
        utils
        types
        parser
        PkgConfig::FUSE3
        PkgConfig::SQLITE3
)