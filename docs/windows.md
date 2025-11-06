# Windows guide

This document captures the recommended tooling and workflows for building the template on Windows hosts.

## Prerequisites
- **Visual Studio 2022** (or Build Tools) with the *Desktop development with C++* workload.
- **Ninja** (recommended): install via [Chocolatey](https://chocolatey.org/) or scoop.
- **Git for Windows**: provides Git Bash for running the repository scripts.
- **vcpkg**: clone `https://github.com/microsoft/vcpkg` and run `bootstrap-vcpkg.bat`.
- **Vulkan SDK** (optional but recommended): install from [LunarG](https://vulkan.lunarg.com/sdk/home#windows).

Configure your environment variables:
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
$env:VULKAN_SDK = "C:\VulkanSDK\<version>"
```
If you installed the SDK system-wide, add `%VULKAN_SDK%\Bin` to `PATH`.

## Configure, build, install, run
Run the scripts from **x64 Native Tools Command Prompt for VS** or **Developer PowerShell** so that `cl.exe` and Ninja are available.

```powershell
bash ./cmake/configure.sh msvc Debug
bash ./build.sh msvc Debug
bash ./install.sh msvc Debug
bash ./run.sh msvc Debug
```

> Tip: `bash` comes with Git for Windows. The scripts are also compatible with WSL.

## Visual Studio workflows
- Open `app1.sln` if you prefer managing the project through a classic solution.
- Alternatively, use **File → Open → CMake...** and select `CMakeLists.txt`. Visual Studio consumes `CMakePresets.json` to mirror the scripted configurations.

## VS Code workflows
1. Install the *C/C++*, *CMake Tools*, and *CMake* extensions.
2. Open the repository folder and select the appropriate kit (MSVC, GCC, or Clang).
3. Use the *CMake: Configure* and *CMake: Build* commands, or call the scripts from the integrated terminal.

## Troubleshooting
- **vcpkg toolchain not detected**: Confirm `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` points to the vcpkg directory before running the configure script.
- **Ninja not found**: Install with `choco install ninja` or remove `ninja.exe` from `PATH` to fallback to `Visual Studio 17 2022` generators.
- **MSVC link errors**: Ensure you are in the correct architecture prompt (`x64 Native Tools`).
- **Vulkan loader errors**: Verify `%VULKAN_SDK%\Bin` and `%VULKAN_SDK%\Lib` are on `PATH` and `%VK_LAYER_PATH%` points at the SDK `Bin` directory.
