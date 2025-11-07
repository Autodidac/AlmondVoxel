find_package(SDL3 CONFIG REQUIRED)

include(${CMAKE_SOURCE_DIR}/tests/test_sources.cmake)

add_executable(terrain_demo terrain_demo.cpp)

target_sources(terrain_demo PRIVATE ${ALMOND_VOXEL_TEST_CASE_SOURCES})

target_link_libraries(terrain_demo PRIVATE almond_voxel SDL3::SDL3)

target_include_directories(terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/tests)

target_compile_options(terrain_demo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
