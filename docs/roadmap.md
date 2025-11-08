# AlmondVoxel roadmap

This roadmap highlights the upcoming work required to evolve AlmondVoxel into a production-ready voxel foundation. Items are grouped by horizon and focus on capabilities that unlock new workflows for downstream engines.

## Table of contents
- [Short-term (next release)](#short-term-next-release)
- [Mid-term (next-quarter)](#mid-term-next-quarter)
- [Long-term (6–12 months)](#long-term-6–12-months)
- [Staying current](#staying-current)

## Short-term (next release)
- **Noise pipeline presets** – ship curated biome tables and noise stacks for desert, tundra, and volcanic regions.
- **Mesh optimisation toggles** – expose compile-time flags for greedy mesher decimation and lightmap UV generation.
- **Streaming sandbox polish** – add frustum culling visualisation, chunk residency overlays, and hot-reloadable configuration files.
- **Documentation uplift** – extend platform guides with performance counters and profiling recipes.

## Mid-term (next quarter)
- **GPU meshing backend** – prototype a compute-shader mesher for Vulkan/DX12 targets with a CPU fallback.
- **Region replication service** – provide an optional background service for synchronising region deltas over the network.
- **Data-oriented storage** – experiment with struct-of-arrays chunk layouts to improve cache behaviour on dense edits.
- **Continuous integration** – automate Linux/Windows builds that compile examples, run `voxel_tests`, and publish artefacts.

## Long-term (6–12 months)
- **ECS bridge** – author adapters for popular ECS frameworks (EnTT, flecs) to stream voxel regions directly into gameplay loops.
- **Editor integration** – package an Unreal/Unity plugin that consumes the header-only library for custom tooling.
- **Procedural authoring toolkit** – build a node-based editor that exports AlmondVoxel configuration packs.
- **Persistent world services** – define schemas and services for cloud-hosted region archives with delta compression.

## Staying current
- Propose roadmap additions or changes through GitHub discussions and link real-world scenarios that benefit from the work.
- Update this document as features ship or scopes change to keep contributors aligned on priorities.
- Reference the roadmap in release notes so downstream projects know when to adopt new modules.
