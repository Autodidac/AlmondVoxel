# Linux guide

This guide covers Ubuntu/Debian-like distributions, but the commands translate to most modern Linux environments.

## Prerequisites
- **Build essentials**: `sudo apt install build-essential cmake ninja-build git`
- **LLVM (optional)**: `sudo apt install clang lld`
- **vcpkg**: clone and bootstrap
  ```bash
  git clone https://github.com/microsoft/vcpkg.git
  ./vcpkg/bootstrap-vcpkg.sh
  export VCPKG_ROOT="$HOME/vcpkg"
  export PATH="$VCPKG_ROOT:$PATH"
  ```
- **Vulkan SDK** (optional): install from [LunarG](https://vulkan.lunarg.com/sdk/home#linux) and export `VULKAN_SDK` and `VK_LAYER_PATH`.

Persist the environment variables by adding them to `~/.bashrc` or `~/.profile`.

## Configure, build, install, run
```bash
./cmake/configure.sh gcc Debug
./build.sh gcc Debug
./install.sh gcc Debug
./run.sh gcc Debug
```

Swap `gcc` for `clang` to use LLVM. Use `Release` or `RelWithDebInfo` for optimized builds.

## VS Code workflows
1. Install the *C/C++*, *CMake Tools*, and *CMake* extensions.
2. Select the GCC or Clang kit exposed by `CMake Tools`.
3. Use the command palette to configure and build, or rely on the scripts above from the integrated terminal.

## Troubleshooting
- **Permission denied running scripts**: make them executable `chmod +x *.sh`.
- **Ninja missing**: install via `sudo apt install ninja-build` or remove `ninja` from `PATH` to fallback to Makefiles.
- **vcpkg triplet errors**: run `./vcpkg/vcpkg integrate install` and ensure the default triplet matches your architecture (`x64-linux`).
- **Vulkan loader errors**: source the `setup-env.sh` script from the Vulkan SDK and reconfigure the project.
