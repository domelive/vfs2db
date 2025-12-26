cmake_minimum_required(VERSION 3.15)

# If running cmake from src/ directory, set up the project environment here
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(vfs2db_standalone VERSION 1.0.0 LANGUAGES C)

    # Set C standard
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    # Enable warnings
    add_compile_options(-Wall -Wextra)

    # Find required packages using pkg-config with IMPORTED_TARGET
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE3 REQUIRED fuse3 IMPORTED_TARGET)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3 IMPORTED_TARGET)
endif()

# Add module subdirectories
add_subdirectory(modules/utils)
add_subdirectory(modules/query_manager)
add_subdirectory(modules/db_handler)
add_subdirectory(modules/syscall_handler)

# Create main executable
add_executable(vfs2db main.c)

# Link against components and external libraries
target_link_libraries(vfs2db
    PRIVATE
        syscall_handler
        db_handler
        query_manager
        utils
        PkgConfig::FUSE3
        PkgConfig::SQLITE3
)