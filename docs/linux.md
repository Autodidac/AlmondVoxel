# Linux guide

This guide targets Debian/Ubuntu-like distributions but generalises to most modern Linux environments. Follow the steps below to install prerequisites, configure the examples, and tune runtime performance.

## Table of contents
- [Prerequisites](#prerequisites)
- [Install toolchain packages](#install-toolchain-packages)
- [Configuration flags](#configuration-flags)
- [Build and test flow](#build-and-test-flow)
- [Performance considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)

## Prerequisites
- **Compiler**: GCC 12+ or Clang 15+ with C++20 support.
- **CMake**: 3.23 or newer for preset compatibility.
- **Ninja** *(optional)*: improves build throughput for the examples/tests.
- **Git**: used to fetch AlmondVoxel as a submodule.

Optional components:
- **Vulkan SDK**: required when profiling the sandbox renderer with GPU capture tools.
- **zstd/lz4 development packages**: provide faster compression backends for `region_io` when the macros enable them.

## Install toolchain packages
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git
```

Install optional compression libraries when toggling non-default backends:
```bash
sudo apt install libzstd-dev liblz4-dev
```

## Configuration flags
The CMake scripts expose several `-D` options when configuring the examples:

| Flag | Default | Description |
| --- | --- | --- |
| `ALMOND_VOXEL_ENABLE_IMGUI` | `ON` | Enables the ImGui UI inside `example_sandbox`. |
| `ALMOND_VOXEL_ENABLE_PROFILING` | `OFF` | Toggles instrumentation for timing chunk/generation stages. |
| `ALMOND_VOXEL_COMPRESSION` | `ZSTD` | Controls compression backend for region serialization (`RAW`, `LZ4`, `ZSTD`). |

Pass them through `configure.sh`:
```bash
./cmake/configure.sh clang Release -DALMOND_VOXEL_ENABLE_PROFILING=ON
```

## Build and test flow
```bash
./cmake/configure.sh gcc Debug
./build.sh gcc Debug voxel_tests
./run.sh gcc Debug voxel_tests
./run.sh gcc Debug example_sandbox
```

`install.sh` packages headers and built binaries into `built/gcc-Debug`, making it easy to drop the artifacts into another project.

## Performance considerations
- Enable `-march=native` by exporting `CXXFLAGS="-march=native"` before running `configure.sh` to leverage CPU-specific instructions.
- Use `ALMOND_VOXEL_CHUNK_SIZE=16` for terrain that favours rapid updates; use `64` for far terrain with fewer edits.
- When profiling, disable ImGui by setting `-DALMOND_VOXEL_ENABLE_IMGUI=OFF` to remove UI overhead.
- For headless benchmarks, build only the CLI example: `./build.sh gcc Release example_streaming`.

## Troubleshooting
| Symptom | Resolution |
| --- | --- |
| **Ninja not found** | Install via `sudo apt install ninja-build` or remove `ninja-build` from the path to fall back to Makefiles. |
| **Missing compression libraries** | Install `libzstd-dev` or `liblz4-dev` when enabling non-default compression modes. |
| **Wayland/OpenGL issues** | Launch the sandbox with `SDL_VIDEODRIVER=x11` if the default video driver fails. |
| **CTest cannot find tests** | Ensure `./build.sh ... voxel_tests` ran before invoking `ctest` inside the build directory. |
