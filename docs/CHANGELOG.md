# Changelog

All notable changes to the AlmondVoxel project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Planned
- Track ongoing demo polish, documentation updates, and additional tooling integrations before the next tagged release.

## [0.9.0] - 2025-11-08
### Added
- Terrain demo upgrades featuring voxel editing tools, physics-driven player movement, and gravity-aware collision handling.
- Generator management panel that allows switching between multiple terrain presets at runtime for rapid iteration.
- Test registrations covering the new voxel modes introduced in the demo so regressions surface quickly.

### Changed
- Streamlined Linux guidance for the real-time demo to ensure SDL3, rendering, and tooling expectations are aligned across platforms.
- Updated showcase assets to highlight the expanded marching cubes pipeline side-by-side with the greedy mesher.

### Fixed
- Resolved include path issues and missing project entries that prevented new headers from compiling in downstream consumers.
- Addressed terrain demo hotfixes for newly added generators to stabilise the experience across builds.

## [0.8.0] - 2025-11-07
### Added
- Marching cubes mesher alongside the existing greedy mesher with seam-aware chunk stitching and iso-surface controls.
- Streaming CLI sample featuring chunked LOD terrain rendering improvements and updated camera controls.
- Continuous lighting tweaks for the marching cubes renderer, including shading controls and viewport adjustments.

### Changed
- Reworked the terrain sandbox to support FPS-style camera navigation and chunk throttling for smoother streaming.
- Optimised marching cubes meshing throughput, chunk scheduling, and render culling for dense worlds.
- Brightened marching cubes shading defaults and enabled test execution from within the demo runtime.

### Fixed
- Patched back-face culling, tearing artefacts, and vertex colour range issues discovered in the SDL3 renderer.
- Corrected marching cubes iso comparisons, orientation mismatches, and regression tests for new voxel modes.
- Addressed multiple hotfixes related to demo stability, asset loading, and build configuration.

## [0.7.0] - 2025-11-06
### Added
- SDL3 renderer conversion for the terrain sandbox with rebuilt Visual Studio solutions and updated tooling files.
- Support for FPS camera controls, higher display resolutions, and configurable chunk LOD streaming.

### Changed
- Refined renderer initialisation, geometry submission, and ImGui overlays to match SDL3 expectations.
- Updated documentation to reflect the SDL3 migration, including refreshed setup notes and troubleshooting tips.

### Fixed
- Resolved terrain demo crashes caused by renderer configuration mismatches and missing assets.
- Applied hotfixes that restored build outputs, corrected git ignore settings, and ensured Ninja project generation succeeds.

## [0.6.0] - 2025-11-06
### Added
- Terrain demo, regression tests, and benchmarks that exercise the almond voxel headers end-to-end.
- Documentation refresh covering architecture, build scripts, and high-level usage guidance.

### Changed
- Removed legacy sample targets from the CMake configuration to focus on the `almond_voxel` interface library and demos.
- Improved build scripts and Visual Studio integration to align with the new project layout.

### Fixed
- Addressed build warnings discovered after reorganising the examples and ensured tests register correctly.

## [0.5.0] - 2025-11-06
### Added
- Almond voxel header library that aggregates core math, chunk storage, generation, meshing, and serialization modules.
- Initial release of helper scripts for configuring, building, installing, and running the examples/tests.

### Changed
- Refined the CMake configuration to expose the `almond_voxel` interface target for external consumers.

## [0.4.0] - 2025-11-06
### Added
- Revised CMake layout and scripts that simplify fetching dependencies and packaging the header-only distribution.

### Fixed
- Corrected build focus to ensure the interface library is front-and-centre for downstream integrations.

## [0.3.0] - 2025-11-06
### Added
- Foundational AlmondVoxel project structure with initial commit scaffolding.

### Changed
- Established repository metadata, licensing, and baseline documentation for contributors.

## Versioning strategy
- Releases follow `MAJOR.MINOR.PATCH` semantics.
- Increment **MAJOR** when making incompatible API or tooling changes.
- Increment **MINOR** when adding functionality in a backward compatible manner.
- Increment **PATCH** for backward compatible bug fixes or documentation updates.
- Each release entry should include the release date and link to comparison diffs when available.

## Maintenance checklist
- Update the `[Unreleased]` section as changes merge into the main branch.
- When cutting a release, create a new section for the version (e.g., `## [1.2.0] - 2026-02-14`) and reset `[Unreleased]` with placeholders.
- Ensure the README, roadmap, and platform guides reflect any notable changes introduced in the release.
