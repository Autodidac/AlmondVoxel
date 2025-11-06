find_package(SDL3 CONFIG REQUIRED)

add_executable(terrain_demo terrain_demo.cpp)

target_link_libraries(terrain_demo PRIVATE almond_voxel SDL3::SDL3)

target_compile_options(terrain_demo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
