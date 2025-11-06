# API overview

The template bundles a small collection of reusable utilities and establishes conventions for adding new libraries. This document summarises what ships today, planned abstractions, and extension points for template consumers.

## Bundled utilities

### `safe_io`
- **Purpose**: provides a type-safe facade around standard output, centralising formatting through `{fmt}`.
- **Key headers**: `shared/safe_io/include/safe_io/utils.hpp` exposes `safe_io::out()` and the templated `safe_io::print()` helper.
- **Linkage**: built as a static library (`safe_io`) and linked into sample applications via CMake (`target_link_libraries(<target> PRIVATE safe_io)`).
- **Usage pattern**: include `<safe_io/utils.hpp>` and call `safe_io::print("Message {}", value);` for buffered, newline-terminated output.

### Static library scaffolding
- **Module**: `StaticLib1` demonstrates how to structure a reusable library with public headers in `StaticLib1/include` and implementation in `StaticLib1/src`.
- **Integration**: its headers are consumed by both `Application1` and `cmakeapp1`, showcasing shared component wiring across different build systems.

## Planned abstractions
- **Configuration layer**: add a lightweight configuration parser (JSON/TOML) with environment overrides and per-target defaults.
- **Logging wrapper**: expand `safe_io` into a logger facade with severity levels, structured context, and sinks for console/files.
- **Platform services**: provide abstractions for file I/O, threading, and GPU context initialisation with unified error handling.
- **Testing helpers**: bundle doctest/catch2 integration with ready-made fixtures and CMake testing presets.

## Extension points
- **New libraries**: mirror `StaticLib1` by creating a directory under `shared/` with `include/` and `src/` subfolders, then register the target in the top-level `CMakeLists.txt`.
- **Application templates**: copy `cmakeapp1` for cross-platform CMake apps or `Application1` for Visual Studio-centric workflows; adjust target linkage as needed.
- **Toolchain hooks**: extend scripts under `cmake/` and `build.sh` to recognise new compilers or build configurations, keeping presets in sync.
- **Documentation**: document new APIs in this file and cross-link any specialised guides under `docs/` so contributors can discover them quickly.

## Maintenance guidelines
- When adding functionality, update this overview to describe new headers, targets, or usage examples.
- Flag experimental APIs with callouts and track their stabilisation in the roadmap and changelog.
- Include code snippets showing best practices for interacting with new abstractions, especially when external dependencies are involved.
