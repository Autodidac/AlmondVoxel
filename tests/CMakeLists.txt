include(test_sources.cmake)

add_executable(almond_voxel_tests test_main.cpp ${ALMOND_VOXEL_TEST_CASE_SOURCES})

target_link_libraries(almond_voxel_tests PRIVATE almond_voxel)

target_include_directories(almond_voxel_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(almond_voxel_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_test(NAME almond_voxel_tests COMMAND almond_voxel_tests)
