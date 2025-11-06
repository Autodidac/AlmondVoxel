add_library(safe_io STATIC
    src/utils.cpp
)

find_package(fmt CONFIG REQUIRED)

# Propagate include directories for both build and install time.
target_include_directories(safe_io
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(safe_io
    PUBLIC
        fmt::fmt
)

# Ensure fmt headers are available to dependents
# (fmt::fmt already carries its include directories).

install(TARGETS safe_io
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)
