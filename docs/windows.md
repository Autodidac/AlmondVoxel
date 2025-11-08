# Windows guide

Use this guide to configure AlmondVoxel on Windows 10/11 for Visual Studio, Visual Studio Code, or command-line workflows.

## Table of contents
- [Prerequisites](#prerequisites)
- [Environment setup](#environment-setup)
- [Configure and build](#configure-and-build)
- [Run demos, benchmarks, and tests](#run-demos-benchmarks-and-tests)
- [Performance notes](#performance-notes)
- [Troubleshooting](#troubleshooting)

## Prerequisites
- **Visual Studio 2022** (Desktop development with C++) or the **MSVC Build Tools 2022**.
- **CMake 3.23+** – installed via the Visual Studio installer or from the official binaries.
- **Ninja** *(recommended)* – install with [Chocolatey](https://community.chocolatey.org/packages/ninja) or [Scoop](https://scoop.sh/). The scripts fall back to the Visual Studio generator if Ninja is unavailable.
- **Git for Windows** – provides Git Bash for running the helper scripts.
- **SDL3 development package** – fetch via [vcpkg](https://github.com/microsoft/vcpkg) (`vcpkg install sdl3`) or the official SDK to build `terrain_demo`.

Optional utilities:
- **Windows Terminal** for improved shell integration.
- **CMake Tools for VS Code** when using VS Code as the IDE.

## Environment setup
Open *x64 Native Tools Command Prompt* or *Developer PowerShell for VS* to ensure MSVC and CMake are on `PATH`:
```powershell
where cmake
where ninja
```

When using Git Bash, forward the MSVC environment first:
```bash
"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat" -arch=x64
```

If SDL3 is installed with vcpkg, point `VCPKG_ROOT` to the installation directory so `configure.sh` picks up the toolchain file automatically.

## Configure and build
From Git Bash or PowerShell (prepend `bash` when running from PowerShell):
```powershell
bash ./cmake/configure.sh msvc Debug
bash ./build.sh msvc Debug
```

To produce Release binaries:
```powershell
bash ./cmake/configure.sh msvc Release
bash ./build.sh msvc Release
```

Install headers and binaries to `built/bin/MSVC-<Config>/`:
```powershell
bash ./install.sh msvc Release
```

Disable optional components during configuration if the SDL3 demo is not needed:
```powershell
bash ./cmake/configure.sh msvc Debug -DALMOND_VOXEL_BUILD_EXAMPLES=OFF
```

## Run demos, benchmarks, and tests
Use the launcher to execute a built target. The script scans `Bin/MSVC-<Config>/` and typical CMake folders:
```powershell
bash ./run.sh terrain_demo
bash ./run.sh almond_voxel_tests
bash ./run.sh mesh_bench
```
Append additional arguments after `--` when launching through `run.sh`, for example `bash ./run.sh terrain_demo -- --mesher=naive` to showcase the naive cubic mesher.

Alternatively, run binaries directly from the build output folder:
```powershell
cd Bin/MSVC-Debug
.\terrain_demo.exe
```

When invoking CTest, stay inside the generated build directory:
```powershell
cd Bin/MSVC-Debug
ctest --output-on-failure
```

## Performance notes
- Enable `/arch:AVX2` or `/arch:ARM64EC` by exporting `CXXFLAGS="/O2 /arch:AVX2"` (or the ARM64 equivalent) before running `configure.sh`.
- For quick iteration, configure once and toggle between Debug/Release using `cmake --build Bin/MSVC-<Config> --config <Debug|Release>`.
- Disable the SDL viewport (`-DALMOND_VOXEL_BUILD_EXAMPLES=OFF`) on headless build agents that do not provide graphics devices.
- Use Windows Performance Analyzer or PIX to profile the SDL3 demo after building with `Release`.

## Troubleshooting
| Symptom | Resolution |
| --- | --- |
| **`configure.sh` cannot locate Ninja** | Install Ninja or remove it from `PATH` to allow the scripts to fall back to the Visual Studio generator. |
| **CMake fails to find SDL3** | Install SDL3 with vcpkg and set `VCPKG_ROOT`, or supply `-DSDL3_DIR=C:/path/to/sdl3/cmake` during configuration. |
| **`run.sh` exits without launching anything** | Ensure `build.sh` completed successfully and that binaries exist under `Bin/MSVC-<Config>/`. Pass the correct target name (e.g., `almond_voxel_tests`). |
| **CTest reports zero tests** | Build once with `bash ./build.sh msvc <Config>` before invoking CTest so the `almond_voxel_tests` binary is generated. |
| **Linker errors referencing `std::filesystem`** | Confirm the MSVC toolset is version 14.36 or newer. Older toolsets shipped with Visual Studio 2019 lack full C++20 support. |
