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

add_executable(classic_heightfield_example classic_heightfield_example.cpp)
target_link_libraries(classic_heightfield_example PRIVATE almond_voxel)
target_compile_options(classic_heightfield_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(greedy_mesher_example greedy_mesher_example.cpp)
target_link_libraries(greedy_mesher_example PRIVATE almond_voxel)
target_compile_options(greedy_mesher_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(marching_cubes_example marching_cubes_example.cpp)
target_link_libraries(marching_cubes_example PRIVATE almond_voxel)
target_compile_options(marching_cubes_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
