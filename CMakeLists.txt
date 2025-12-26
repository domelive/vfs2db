cmake_minimum_required(VERSION 3.15)
project(vfs2db VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable warnings
add_compile_options(-Wall -Wextra)

# Find required packages using pkg-config with IMPORTED_TARGET
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3 IMPORTED_TARGET)
pkg_check_modules(SQLITE3 REQUIRED sqlite3 IMPORTED_TARGET)

# Add subdirectories
add_subdirectory(src)