# API overview

The template bundles a small collection of reusable utilities and establishes conventions for adding new libraries. This document summarises what ships today, planned abstractions, and extension points for template consumers.

## Bundled utilities

### `almond_voxel`
- **Purpose**: header-only voxel toolset providing chunk storage, region management, procedural generation, greedy meshing, and binary serialization primitives.
- **Key headers**: aggregated through `include/almond_voxel/almond_voxel.hpp`, which exposes `core.hpp`, `chunk.hpp`, `world.hpp`, `generation/noise.hpp`, `meshing/greedy_mesher.hpp`, and `serialization/region_io.hpp`.
- **Integration**: exported as an `INTERFACE` target named `almond_voxel` and linked into sample applications with `target_link_libraries(<target> PRIVATE almond_voxel)`.
- **Usage pattern**: create a `chunk_storage`, populate voxels (optionally via `generation::value_noise`), manage residency with `region_manager`, and emit meshes through `meshing::greedy_mesh` before persisting regions using the serialization helpers.

## Planned abstractions
- **Configuration layer**: add a lightweight configuration parser (JSON/TOML) with environment overrides and per-target defaults.
- **Meshing improvements**: extend the greedy mesher with material-aware UV packing, LOD generation, and instanced rendering support.
- **Generation pipeline**: add biome combinators, signed-distance remappers, and erosion passes layered on top of the current noise samplers.
- **Platform services**: provide abstractions for file I/O, threading, and GPU context initialisation with unified error handling.
- **Testing helpers**: bundle doctest/catch2 integration with ready-made fixtures and CMake testing presets.

## Extension points
- **New libraries**: follow the `almond_voxel` pattern by exposing headers under `include/` and registering an `INTERFACE` target in the top-level `CMakeLists.txt`.
- **Application templates**: copy `cmakeapp1` for cross-platform CMake apps or `Application1` for Visual Studio-centric workflows; adjust target linkage as needed.
- **Toolchain hooks**: extend scripts under `cmake/` and `build.sh` to recognise new compilers or build configurations, keeping presets in sync.
- **Documentation**: document new APIs in this file and cross-link any specialised guides under `docs/` so contributors can discover them quickly.

## Maintenance guidelines
- When adding functionality, update this overview to describe new headers, targets, or usage examples.
- Flag experimental APIs with callouts and track their stabilisation in the roadmap and changelog.
- Include code snippets showing best practices for interacting with new abstractions, especially when external dependencies are involved.
