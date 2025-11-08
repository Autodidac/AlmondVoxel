set(ALMOND_VOXEL_TEST_CASE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/amalgamated_smoke.cpp
    ${CMAKE_CURRENT_LIST_DIR}/chunk_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/editing_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/meshing_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/navigation_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serialization_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/terrain_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/raytracing_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/world_tests.cpp
    CACHE INTERNAL "Almond Voxel unit test sources"
)
