
# Set the vcpkg toolchain file
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg_installed/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(cmakeapp1 VERSION 1.0
                  DESCRIPTION "My Project Description"
                  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce install location at top-level (in the project root)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}\\built" CACHE PATH "Install path prefix" FORCE)
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")


# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Set compile options
target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)


# Find and link dependencies
find_package(Threads QUIET)

target_link_libraries(${PROJECT_NAME} PRIVATE safe_io)

if(Threads_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# Install rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Ensure libraries are installed (if applicable)
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Fix missing symbols for GNU/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--allow-shlib-undefined)
endif()

