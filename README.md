# Cpp20_Ultimate_Project_Template

The Ultimate Hello World – cross-platform, multi-editor ready C++20 template for quickly bootstrapping real-world applications and libraries.

## Project vision
- Provide a batteries-included baseline for modern C++20 development that works across Windows and Linux.
- Showcase clean CMake integration with manifest-based dependency management (vcpkg) and graphics-ready defaults (Vulkan SDK).
- Offer repeatable scripts so teams can configure, build, install, and run projects from a single source of truth.
- Remain easy to extend to new compilers, targets, and IDE workflows.

## Supported toolchains
| Compiler | Generator | Notes |
| --- | --- | --- |
| GCC | Ninja or Unix Makefiles | Auto-detected by scripts. Works on Linux and Windows (via MSYS2/MinGW). |
| Clang | Ninja or Unix Makefiles | Uses system `clang++`; compatible with LLVM toolchains on all platforms. |
| MSVC | Ninja Multi-Config | Requires Visual Studio Build Tools and an MSVC developer prompt. |

> Configure-time detection automatically injects the appropriate vcpkg toolchain if `VCPKG_ROOT`, `VCPKG_INSTALLATION_ROOT`, or a local `vcpkg_installed` folder is present.

## Quick start
Follow the configure → build → install → run flow:

1. **Configure**
   ```bash
   ./cmake/configure.sh gcc Debug
   ```
   Adjust the compiler (`gcc`, `clang`, `msvc`) and build type (`Debug`, `Release`, etc.) as needed. Configuration output is placed in `Bin/<Compiler>-<Config>`.

2. **Build**
   ```bash
   ./build.sh gcc Debug
   ```
   Reuses the configuration directory if it already exists. Pass `Release` to build optimized binaries.

3. **Install**
   ```bash
   ./install.sh gcc Debug
   ```
   Installs artifacts into `built/bin/<Compiler>-<Config>` so they can be redistributed or consumed by downstream projects.

4. **Run**
   ```bash
   ./run.sh gcc Debug
   ```
   Automatically locates the configured executable (e.g., `cmakeapp1`) and launches it.

Platform-specific setup steps (Visual Studio, VS Code tasks, Linux package managers, etc.) are documented in:
- [docs/windows.md](docs/windows.md)
- [docs/linux.md](docs/linux.md)

Additional high-level documentation lives in:
- [docs/roadmap.md](docs/roadmap.md) for upcoming enhancements and priorities.
- [docs/api-overview.md](docs/api-overview.md) for bundled utilities, abstractions, and extension points.
- [docs/CHANGELOG.md](docs/CHANGELOG.md) for release notes and versioning strategy.

## Environment prerequisites
- **vcpkg**: Required for dependency resolution. Set either `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` before configuring, or place a local bootstrap under `vcpkg_installed/`.
- **Vulkan SDK**: Install the Vulkan SDK for graphics samples or libraries that rely on Vulkan. Ensure `VULKAN_SDK` is exported and its `Bin` directory is in `PATH`.
- **CMake ≥ 3.21**: Needed for multi-config generators and preset support.
- **Ninja (optional)**: Speeds up builds; automatically selected when available.

See the platform guides for package manager recommendations and verification commands.

## Automation scripts
All helper scripts reside at the repository root and are POSIX shell compatible (Windows users should run them inside WSL, MSYS2, or Git Bash).

| Script | Purpose | Usage examples |
| --- | --- | --- |
| `cmake/configure.sh` | Creates the build tree for a compiler + configuration pair. | `./cmake/configure.sh clang Release`<br>`./cmake/configure.sh msvc RelWithDebInfo` |
| `build.sh` | Builds the selected target set. | `./build.sh gcc Debug` |
| `install.sh` | Installs CMake targets into `built/bin/<Compiler>-<Config>`. | `./install.sh clang Release` |
| `run.sh` | Locates and executes the sample executable for the requested configuration. | `./run.sh msvc Release` |

### Troubleshooting
- **Missing vcpkg toolchain**: Ensure `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` points to a valid vcpkg clone. Re-run `./cmake/configure.sh ...` after setting the variable.
- **Generator errors**: Install Ninja (`sudo apt install ninja-build` or `choco install ninja`) or ensure `make` is available. The scripts will fallback to Unix Makefiles when Ninja is absent.
- **MSVC builds fail outside developer prompt**: Use the “x64 Native Tools Command Prompt for VS” so that `cl.exe` and Ninja are on `PATH`.
- **Vulkan loader not found**: Confirm the Vulkan SDK is installed and that `VK_LAYER_PATH` and `VULKAN_SDK` environment variables are exported before configuring.

## Customizing for new targets
1. **Extend the configuration script**: Update `cmake/configure.sh` with a new compiler case (e.g., `icc`, `emcc`) and provide generator/toolchain arguments.
2. **Add CMake presets**: Edit `CMakePresets.json` to encode the new build matrix for IDEs that support presets.
3. **Update automation scripts**: Mirror the new compiler identifier in `build.sh`, `install.sh`, and `run.sh` so the end-to-end flow is consistent.
4. **Document the workflow**: Add a subsection to the appropriate platform guide (`docs/*.md`) describing unique requirements or environment variables.

## Contribution guidelines
- **Roadmap**: When proposing or completing notable work, update [docs/roadmap.md](docs/roadmap.md) to reflect new priorities or delivered items.
- **API overview**: Document new libraries, utilities, or extension points in [docs/api-overview.md](docs/api-overview.md) with usage notes.
- **Changelog**: Record user-visible changes under the `[Unreleased]` section of [docs/CHANGELOG.md](docs/CHANGELOG.md) and promote entries during releases.
- **Cross-links**: Ensure new documentation is linked from the README or relevant guides so contributors can discover it easily.

## Licensing
This template ships with the default license for demonstration purposes. Replace it with your organization’s preferred license before distributing derived work.
