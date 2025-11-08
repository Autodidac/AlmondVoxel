# Changelog

All notable changes to AlmondVoxel are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Integrated the naive cubic mesher option into `terrain_demo`, making it available alongside the greedy and marching paths.

### Changed
- Refreshed documentation to match the current demos, tests, and cross-platform build scripts.
- Clarified maintenance expectations and removed legacy contribution guidance.
- Corrected chunk selection to prioritise nearby regions when scaling render distance.

## [0.1.0] - 2023-11-01
### Added
- Initial header-only voxel toolkit covering core math, chunk storage, region streaming, terrain sampling, meshing, serialization, and editing helpers.
- CMake interface target `almond_voxel` with options to build examples, tests, and benchmarks.
- Runnable demos: `terrain_demo`, `classic_heightfield_example`, `cubic_naive_mesher_example`, `greedy_mesher_example`, and `marching_cubes_example`.
- Benchmark target `mesh_bench` for greedy meshing throughput measurements.
- `almond_voxel_tests` executable aggregating all unit tests.
- Helper scripts (`cmake/configure.sh`, `build.sh`, `install.sh`, `run.sh`) for consistent workflows on Linux, macOS, and Windows.
- Documentation set including platform guides, API overview, roadmap, and tooling inventory.

## Versioning strategy
- Releases follow `MAJOR.MINOR.PATCH` semantics.
- Increment **MAJOR** when making incompatible API or tooling changes.
- Increment **MINOR** when adding functionality in a backward compatible manner.
- Increment **PATCH** for backward compatible bug fixes or documentation updates.
- Each release entry should include the release date and link to comparison diffs when available.

## Maintenance checklist
- Update the `[Unreleased]` section as changes merge into the main branch.
- When cutting a release, create a new version section and reset `[Unreleased]` with placeholders.
- Ensure the README, platform guides, and API overview reflect notable changes introduced in the release.
