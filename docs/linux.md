# Linux & macOS guide

This guide covers Debian/Ubuntu, Fedora, and macOS environments. The same steps apply to other Unix-like platforms with equivalent package names.

## Table of contents
- [Prerequisites](#prerequisites)
- [Install packages](#install-packages)
- [Configure and build](#configure-and-build)
- [Run demos, benchmarks, and tests](#run-demos-benchmarks-and-tests)
- [Performance considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)

## Prerequisites
- **Compiler**: GCC 12+/Clang 15+ with C++20 support.
- **CMake**: 3.23 or newer.
- **Ninja** *(optional)*: preferred generator for faster builds. The scripts fall back to Makefiles when Ninja is absent.
- **SDL3 development headers**: required by `terrain_demo`. Install `libsdl3-dev` on Debian/Ubuntu, `libsdl3-devel` on Fedora, or `brew install sdl3` on macOS.
- **Git**: to clone/update AlmondVoxel and drive Git Bash on macOS if desired.

Optional but helpful:
- **Python 3**: useful for quick data-prep or benchmarking scripts.
- **Ccache**: speeds up repeated rebuilds of the demo and test binaries.

## Install packages
### Debian/Ubuntu
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libsdl3-dev git
```

### Fedora/RHEL
```bash
sudo dnf install gcc-c++ clang cmake ninja-build SDL3-devel git
```

### macOS (Homebrew)
```bash
brew install cmake ninja llvm sdl3 git
```
Add the brewed Clang to your `PATH` when building with it:
```bash
export CC=$(brew --prefix llvm)/bin/clang
export CXX=$(brew --prefix llvm)/bin/clang++
```

## Configure and build
Create the build tree with the helper script:
```bash
./cmake/configure.sh clang Debug
# or ./cmake/configure.sh gcc Release
```

Compile all targets for the selected configuration:
```bash
./build.sh clang Debug
```

To install headers and executables under `built/bin/<Compiler>-<Config>/`:
```bash
./install.sh clang Debug
```

Disable optional components during configuration if you do not need them:
```bash
./cmake/configure.sh gcc Release -DALMOND_VOXEL_BUILD_EXAMPLES=OFF
```

## Run demos, benchmarks, and tests
The `run.sh` helper searches common build folders for the requested binary:
```bash
./run.sh terrain_demo
./run.sh cubic_naive_mesher_example
./run.sh greedy_mesher_example
./run.sh mesh_bench
```

Execute the test suite either through `run.sh` or directly via CTest:
```bash
./run.sh almond_voxel_tests
# or
cd Bin/Clang-Debug
ctest --output-on-failure
```

To run only headless components in CI or remote servers, disable the SDL3 demo during configuration and invoke `classic_heightfield_example`, `cubic_naive_mesher_example`, `greedy_mesher_example`, `marching_cubes_example`, `mesh_bench`, and `almond_voxel_tests` manually.

## Performance considerations
- Export `CXXFLAGS="-O3 -march=native"` (or `-mcpu=native` on Apple Silicon) before configuring to enable CPU-specific optimisations.
- Lower chunk dimensions (e.g., `chunk_extent{16, 16, 16}`) accelerate meshing and editing loops when prototyping interactive tools.
- Use `mesh_bench` to evaluate greedy meshing throughput across compiler flags or architecture changes.
- When profiling `terrain_demo`, run it with `SDL_VIDEODRIVER=x11` on Wayland setups to avoid driver throttling.

## Troubleshooting
| Symptom | Resolution |
| --- | --- |
| **CMake cannot find SDL3** | Confirm the development package is installed. Provide an explicit `CMAKE_PREFIX_PATH` (e.g., `/usr/lib/cmake/SDL3`) or set `SDL3_DIR` before running `configure.sh`. |
| **`run.sh` reports “No runnable example was found”** | Ensure the build step succeeded and that binaries live under `Bin/<Compiler>-<Config>/`. Pass the compiler/configuration used during `configure.sh` to `build.sh`. |
| **Linker errors referencing `std::filesystem`** | Build with GCC 12+ or Clang 15+; older toolchains lack the required standard library support. |
| **macOS codesign prompt when launching `terrain_demo`** | The demo is unsigned; approve the dialog in *System Settings → Privacy & Security* and rerun. |
| **CTest exits with zero tests** | Build once with `./build.sh <compiler> <config>` to generate `almond_voxel_tests` before invoking CTest inside the build directory. |
