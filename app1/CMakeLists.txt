
# Set the vcpkg toolchain file
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg_installed/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

# Create executable for vulkanapp1
add_executable(app1
    src/main.cpp
)

target_link_libraries(app1 PRIVATE
    safe_io
)

# Installation rule
install(TARGETS app1
    RUNTIME DESTINATION bin
)

# Fix missing symbols for GNU/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(app1 PRIVATE -Wl,--allow-shlib-undefined)
endif()
