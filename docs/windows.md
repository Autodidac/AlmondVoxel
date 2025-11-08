# Windows guide

Use this guide to configure AlmondVoxel on Windows 10/11 for Visual Studio, VS Code, or command-line workflows.

## Table of contents
- [Prerequisites](#prerequisites)
- [Environment setup](#environment-setup)
- [Configure and build](#configure-and-build)
- [Configuration flags](#configuration-flags)
- [Performance notes](#performance-notes)
- [Troubleshooting](#troubleshooting)

## Prerequisites
- **Visual Studio 2022** (Desktop development with C++) or the **MSVC Build Tools**.
- **CMake 3.23+** installed via the Visual Studio installer or the official binaries.
- **Ninja** *(optional)* from [Chocolatey](https://community.chocolatey.org/packages/ninja) or [Scoop](https://scoop.sh/).
- **Git for Windows** to provide Git Bash and Unix-compatible shells.

Optional components:
- **Windows SDK graphics tools** if you plan to run the ImGui sandbox with GPU capture.
- **vcpkg** only when experimenting with custom dependencies inside the examples—AlmondVoxel itself has none.

## Environment setup
Open *Developer PowerShell for VS* or *x64 Native Tools Command Prompt* and ensure the tools are accessible:
```powershell
where cmake
where ninja
```

If using Git Bash, forward the MSVC toolchain environment:
```bash
"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat" -arch=x64
```

## Configure and build
```powershell
bash ./cmake/configure.sh msvc Debug -DALMOND_VOXEL_ENABLE_IMGUI=ON
bash ./build.sh msvc Debug example_sandbox
bash ./run.sh msvc Debug example_sandbox
```

To install headers and binaries for redistribution:
```powershell
bash ./install.sh msvc Release
```

## Configuration flags
The same CMake flags as Linux apply. To pass multiple flags from PowerShell:
```powershell
bash ./cmake/configure.sh msvc Release `
    -DALMOND_VOXEL_COMPRESSION=LZ4 `
    -DALMOND_VOXEL_ENABLE_PROFILING=ON
```

## Performance notes
- Enable `/arch:AVX2` by setting `CXXFLAGS` before running `configure.sh` for chunk-heavy workloads.
- When profiling on laptops, disable ImGui (`-DALMOND_VOXEL_ENABLE_IMGUI=OFF`) to keep GPU usage low.
- Use the Visual Studio *Graphics Analyzer* to inspect the sandbox renderer after launching via `run.sh`.

## Troubleshooting
| Symptom | Resolution |
| --- | --- |
| **CMake cannot find the compiler** | Ensure the prompt is initialised with `VsDevCmd.bat` before calling the scripts. |
| **Ninja build files missing** | Install Ninja or allow the scripts to fall back to the Visual Studio generator by removing `ninja.exe` from `PATH`. |
| **Sandbox window fails to create** | Update GPU drivers and verify that the Windows SDK graphics tools are installed. |
| **Doctest runner has no output** | Use `./run.sh ... voxel_tests -- --reporters=console` to force console output. |
