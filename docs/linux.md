# Linux guide

This guide targets Debian/Ubuntu-like distributions but generalises to most modern Linux environments.

## Prerequisites
- **Compiler**: GCC 12+ or Clang 15+ with C++20 support.
- **CMake**: 3.23 or newer for preset compatibility.
- **Ninja** (optional): improves build throughput for the examples/tests.
- **Git**: used to fetch AlmondVoxel as a submodule.

Because AlmondVoxel is header-only, no packaged dependencies (vcpkg, Vulkan SDK) are required to embed the library. Optional components:
- **Vulkan SDK**: only needed when building the sandbox renderer with GPU profiling enabled.
- **zstd/lz4 dev packages**: provide faster compression backends for `region_io` when the macros enable them.

Install tooling via apt:
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git
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
- **Ninja not found**: remove `ninja-build` from the path or install it via `sudo apt install ninja-build`.
- **Missing compression libraries**: install `libzstd-dev` or `liblz4-dev` when enabling non-default compression modes.
- **Wayland/OpenGL issues**: launch the sandbox with `SDL_VIDEODRIVER=x11` if the default video driver fails.
- **CTest cannot find tests**: ensure `./build.sh ... voxel_tests` ran before invoking `ctest` inside the build directory.
