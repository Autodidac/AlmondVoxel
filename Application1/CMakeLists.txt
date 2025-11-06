
# Set the vcpkg toolchain file
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg_installed/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

# Add executable
add_executable(Application1
    src/Application1.cpp
)

# Include headers and link to library
target_include_directories(Application1 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(Application1 PRIVATE
    almond_voxel
)

# If you're targeting X11, make sure that the X11-specific source is added conditionally
if(UNIX AND NOT APPLE)  # Assuming this targets Linux/X11
   # target_sources(Static PRIVATE ../../src/Linux.cpp)
endif()

# Installation rule for the executable
install(TARGETS Application1
    RUNTIME DESTINATION bin
)
