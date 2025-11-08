# API overview

This document catalogues the AlmondVoxel headers, describes the interface target exported by CMake, and highlights recommended usage patterns for demos, internal tooling, and automated tests.

## Table of contents
- [Module map](#module-map)
- [Interface target](#interface-target)
- [Usage patterns](#usage-patterns)
  - [Streaming regions](#streaming-regions)
  - [Greedy mesh extraction](#greedy-mesh-extraction)
  - [Marching cubes surfaces](#marching-cubes-surfaces)
  - [Terrain sampling](#terrain-sampling)
  - [Serialization](#serialization)
  - [Editing helpers](#editing-helpers)
- [Testing harness](#testing-harness)
- [Internal extensions](#internal-extensions)

## Module map
All headers live under `include/almond_voxel/`. Include `almond_voxel/almond_voxel.hpp` for the full toolkit or pull in the modules you need individually.

| Header | Description | Key types/functions |
| --- | --- | --- |
| `almond_voxel/core.hpp` | Fundamental voxel/value types, extent utilities, and `span3d` helpers. | `voxel_id`, `chunk_extent`, `cubic_extent`, `span3d` |
| `almond_voxel/chunk.hpp` | Chunk storage with lighting/metadata channels, compression hooks, and dirty tracking. | `chunk_storage`, `chunk_extent::volume`, `chunk_storage::set_compression_hooks` |
| `almond_voxel/world.hpp` | Region streaming, pinning, loader/saver callbacks, and task scheduling. | `region_manager`, `region_key`, `region_manager::tick` |
| `almond_voxel/generation/noise.hpp` | Deterministic value noise and palette utilities for procedural generation. | `generation::value_noise`, `palette_builder`, `palette_entry` |
| `almond_voxel/terrain/classic.hpp` | Classic layered terrain sampler suitable for demo height fields. | `terrain::classic_heightfield`, `terrain::classic_config` |
| `almond_voxel/editing/voxel_editing.hpp` | Brush operations for carving or filling regions. | `editing::apply_sphere`, `editing::apply_box`, `editing::visit_region` |
| `almond_voxel/meshing/mesh_types.hpp` | Vertex/index containers used by meshing routines. | `meshing::mesh_buffer`, `meshing::vertex` |
| `almond_voxel/meshing/greedy_mesher.hpp` | Greedy mesher producing blocky triangle meshes from chunk data. | `meshing::greedy_mesh` |
| `almond_voxel/meshing/marching_cubes.hpp` | Iso-surface mesher for smooth terrain. | `meshing::marching_cubes`, `meshing::marching_cubes_from_chunk` |
| `almond_voxel/serialization/region_io.hpp` | Binary snapshot helpers for regions and chunk payloads. | `serialization::serialize_chunk`, `serialization::make_region_serializer` |
| `tests/test_framework.hpp` | Lightweight assertion/registration utilities shared by examples and tests. | `TEST_CASE`, `CHECK`, `run_tests` |

## Interface target
AlmondVoxel installs as an `INTERFACE` library. Downstream projects inherit include paths automatically and do not link against a compiled binary.

```cmake
add_subdirectory(external/AlmondVoxel)

add_executable(worldgen main.cpp)
target_link_libraries(worldgen PRIVATE almond_voxel)
```

When the repository is used directly, the helper scripts configure the `almond_voxel` target along with demos, tests, and benchmarks. To exclude optional components, pass the relevant `-DALMOND_VOXEL_BUILD_*` cache variables to `cmake/configure.sh` or the underlying CMake invocation.

## Usage patterns
### Streaming regions
```cpp
#include <almond_voxel/world.hpp>
#include <almond_voxel/terrain/classic.hpp>

almond::voxel::region_manager manager{almond::voxel::cubic_extent(32)};
almond::voxel::terrain::classic_heightfield generator{};

manager.set_loader([&](const almond::voxel::region_key& key) {
    return generator(key);
});

manager.set_max_resident(64);
manager.pin({0, 0, 0});
manager.enqueue_task({0, 0, 0}, [](auto& chunk, auto) {
    chunk.fill(almond::voxel::voxel_id{2});
});
manager.tick();
```

### Greedy mesh extraction
```cpp
#include <almond_voxel/meshing/greedy_mesher.hpp>
#include <almond_voxel/chunk.hpp>

almond::voxel::chunk_storage chunk{almond::voxel::cubic_extent(32)};
chunk.fill(almond::voxel::voxel_id{1});
const auto mesh = almond::voxel::meshing::greedy_mesh(chunk);
```

### Marching cubes surfaces
```cpp
#include <almond_voxel/meshing/marching_cubes.hpp>
#include <almond_voxel/generation/noise.hpp>

almond::voxel::chunk_storage chunk{almond::voxel::cubic_extent(32)};
auto voxels = chunk.voxels();
almond::voxel::generation::value_noise noise{1337, 0.04};
for (std::uint32_t z = 0; z < chunk.extent().z; ++z) {
    for (std::uint32_t y = 0; y < chunk.extent().y; ++y) {
        for (std::uint32_t x = 0; x < chunk.extent().x; ++x) {
            const double density = noise.sample(x, y, z);
            voxels(x, y, z) = density > 0.0 ? almond::voxel::voxel_id{1} : almond::voxel::voxel_id{};
        }
    }
}
const auto smooth_mesh = almond::voxel::meshing::marching_cubes_from_chunk(chunk);
```

### Terrain sampling
```cpp
#include <almond_voxel/terrain/classic.hpp>

almond::voxel::terrain::classic_heightfield terrain{};
auto chunk = terrain({0, 0, 0});
double height = terrain.sample_height(32.5, 48.0);
```

### Serialization
```cpp
#include <almond_voxel/serialization/region_io.hpp>

almond::voxel::chunk_storage chunk{almond::voxel::cubic_extent(16)};
chunk.fill(almond::voxel::voxel_id{3});

std::vector<std::byte> payload = almond::voxel::serialization::serialize_chunk(chunk);
auto restored = almond::voxel::serialization::deserialize_chunk(payload);
```

### Editing helpers
```cpp
#include <almond_voxel/editing/voxel_editing.hpp>
#include <almond_voxel/chunk.hpp>

almond::voxel::chunk_storage chunk{almond::voxel::cubic_extent(32)};
almond::voxel::editing::apply_sphere(chunk, {16, 16, 16}, 10.0f, almond::voxel::voxel_id{5});
```

## Testing harness
`tests/test_framework.hpp` exposes macros that register tests automatically. The `almond_voxel_tests` executable pulls in all sources listed in `tests/test_sources.cmake` and calls `almond::voxel::test::run_tests()` from `tests/test_main.cpp`.

When adding new test files, list them in `tests/test_sources.cmake` and include `tests/test_framework.hpp` to gain access to `TEST_CASE`, `CHECK`, and related helpers.

## Internal extensions
New headers or features should:

1. Live under an appropriate domain folder inside `include/almond_voxel/` and be wired into `almond_voxel/almond_voxel.hpp` if they are part of the public interface.
2. Provide a focused example or usage snippet in `examples/` when applicable.
3. Register coverage under `tests/` so automated builds exercise the behaviour.
4. Update the documentation set (README, platform guides, and this overview) so the new functionality is discoverable.

The project is curated internally; external change requests are not accepted.
