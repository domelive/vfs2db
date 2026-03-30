# Utils is a header-only library (interface library)
add_library(utils INTERFACE)

# Specify the public include directory
target_include_directories(utils
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(utils
    INTERFACE
        logger
        db_handler
        types
)