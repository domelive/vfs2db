cmake_minimum_required(VERSION 3.15)

# Add module subdirectories
add_subdirectory(modules/utils)
add_subdirectory(modules/query_manager)
add_subdirectory(modules/db_handler)
add_subdirectory(modules/syscall_handler)
add_subdirectory(modules/cache_manager)

# Create main executable
add_executable(vfs2db main.c)

# Link against components and external libraries
target_link_libraries(vfs2db
    PRIVATE
        syscall_handler
        db_handler
        query_manager
        utils
        cache_manager
        PkgConfig::FUSE3
        PkgConfig::SQLITE3
)