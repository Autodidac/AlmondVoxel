# AlmondVoxel roadmap

This roadmap captures internal priorities for evolving AlmondVoxel. It is published for visibility only; timelines may shift based on internal needs.

## Table of contents
- [Short-term (next release)](#short-term-next-release)
- [Mid-term (next quarter)](#mid-term-next-quarter)
- [Long-term (6–12 months)](#long-term-6–12-months)
- [Staying aligned](#staying-aligned)

## Short-term (next release)
- **Region compression adapters** – provide ready-made zlib/zstd adapters for `chunk_storage::set_compression_hooks`.
- **Terrain demo polish** – expose brush presets, add camera path recording, and surface debug overlays (FPS, residency, edit history).
- **Serialization regression tests** – extend `almond_voxel_tests` with round-trip cases covering metadata/skylight channels.
- **Documentation cadence** – ensure new modules are reflected in the API overview and platform guides within the same release window.

## Mid-term (next quarter)
- **GPU meshing prototype** – evaluate compute-driven greedy meshing for Vulkan/DX12 backends with CPU fallback.
- **Region streaming metrics** – integrate optional instrumentation to log LRU churn, task budgets, and average loader latency.
- **CLI tooling** – add a headless snapshot utility that exercises serialization and editing helpers for automated pipelines.
- **Continuous integration** – stand up GitHub Actions runners for GCC, Clang, and MSVC to build demos/tests on every push.

## Long-term (6–12 months)
- **Authoring toolkit** – explore a node-based editor for composing terrain generators that export AlmondVoxel configuration packs.
- **Engine adapters** – prototype bridges for ECS/gameplay frameworks to consume `region_manager` snapshots efficiently.
- **Streaming services** – design a persistence service for multi-user worlds with delta compression and conflict resolution.
- **Extended lighting model** – research voxel GI probes and multiple shadow channels for advanced demos.

## Staying aligned
- Review this roadmap when planning releases so feature work, documentation, and tests land together.
- Capture major changes in `docs/CHANGELOG.md` and summarise the impact in the README when user-facing behaviour shifts.
- This project is maintained privately; roadmap updates are published for awareness rather than public contribution planning.
