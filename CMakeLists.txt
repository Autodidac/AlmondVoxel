# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
#cmake_minimum_required(VERSION 3.3[...<3.8>] [FATAL_ERROR])

project(AlmondVoxel VERSION 0.1.0
                  DESCRIPTION "Almond Voxel interface library"
                  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# (Optional) Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Enforce install location at top-level (in the project root)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/built" CACHE PATH "Install path prefix" FORCE)
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")

# Global output directories
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG     "${CMAKE_BINARY_DIR}/lib/Debug")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/lib/Release")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG     "${CMAKE_BINARY_DIR}/bin/Debug")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/bin/Release")

# Configure vcpkg installation root
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    set(VCPKG_INSTALLATION_ROOT "$ENV{VCPKG_INSTALLATION_ROOT}/vcpkg_installed")
else()
    set(VCPKG_INSTALLATION_ROOT "${CMAKE_SOURCE_DIR}/vcpkg_installed")
endif()

add_library(almond_voxel INTERFACE)
target_compile_features(almond_voxel INTERFACE cxx_std_20)
target_include_directories(almond_voxel
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/almond_voxel>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/almond_voxel>
)

option(ALMOND_VOXEL_BUILD_EXAMPLES "Build Almond Voxel example targets" ON)
option(ALMOND_VOXEL_BUILD_TESTS "Build Almond Voxel unit tests" ON)
option(ALMOND_VOXEL_BUILD_BENCHMARKS "Build Almond Voxel benchmark targets" ON)

option(ALMOND_VOXEL_BUILD_AMALGAMATED "Generate and install the amalgamated single header" OFF)

if(ALMOND_VOXEL_BUILD_AMALGAMATED)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(ALMOND_VOXEL_AMALGAMATED_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/almond_voxel_single.hpp)

    file(GLOB_RECURSE ALMOND_VOXEL_PUBLIC_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/almond_voxel/*.hpp
    )

    add_custom_command(
        OUTPUT ${ALMOND_VOXEL_AMALGAMATED_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_amalgamated_header.py --output ${ALMOND_VOXEL_AMALGAMATED_OUTPUT}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_amalgamated_header.py
            ${ALMOND_VOXEL_PUBLIC_HEADERS}
        COMMENT "Generating almond_voxel_single.hpp amalgamation"
        VERBATIM
    )

    add_custom_target(almond_voxel_amalgamated_header ALL
        DEPENDS ${ALMOND_VOXEL_AMALGAMATED_OUTPUT}
    )

    target_include_directories(almond_voxel
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    )

    target_compile_definitions(almond_voxel INTERFACE ALMOND_VOXEL_USE_AMALGAMATED_HEADER)
endif()

if(ALMOND_VOXEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ALMOND_VOXEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ALMOND_VOXEL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

install(TARGETS almond_voxel EXPORT almond_voxelTargets)
install(DIRECTORY include/ DESTINATION include
    PATTERN "almond_voxel_single.hpp" EXCLUDE
)

if(ALMOND_VOXEL_BUILD_AMALGAMATED)
    install(FILES ${ALMOND_VOXEL_AMALGAMATED_OUTPUT} DESTINATION include RENAME almond_voxel_single.hpp)
else()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/almond_voxel_single.hpp DESTINATION include)
endif()
















