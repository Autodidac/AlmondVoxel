add_executable(mesh_bench mesh_bench.cpp)

target_link_libraries(mesh_bench PRIVATE almond_voxel)

target_compile_options(mesh_bench PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(raytracing_bench raytracing_bench.cpp)

target_link_libraries(raytracing_bench PRIVATE almond_voxel)

target_compile_options(raytracing_bench PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
