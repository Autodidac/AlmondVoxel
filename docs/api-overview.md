# API overview

This document catalogues the AlmondVoxel headers, the CMake interface they expose, and recommended patterns for embedding the toolkit in your voxel engine or game.

## Module map

| Header | Description | Key types/functions | Configuration flags |
| --- | --- | --- | --- |
| `almond_voxel/core.hpp` | Core math utilities, voxel traits, axis-aligned helpers. | `voxel_id`, `chunk_index`, `axis_extent`, `wrap_coordinates`. | `ALMOND_VOXEL_COORD_WRAP` toggles modulo wrapping across world bounds. |
| `almond_voxel/chunk.hpp` | Sparse chunk containers with cache-aware storage backends. | `chunk_storage`, `chunk_ref`, `chunk_layer`. | `ALMOND_VOXEL_CHUNK_SIZE` (default 32) controls edge length. |
| `almond_voxel/world.hpp` | Region manager orchestrating chunk residency and eviction. | `region_id`, `region_manager`, `region_ticket`. | `ALMOND_VOXEL_MAX_RESIDENT` limits total loaded chunks. |
| `almond_voxel/generation/noise.hpp` | Noise functions and combinators for terrain density. | `value_noise`, `ridged_noise`, `fractal_combine`. | `ALMOND_VOXEL_NOISE_FREQUENCY`, `ALMOND_VOXEL_SEED`. |
| `almond_voxel/generation/biomes.hpp` | Biome layering helpers for climate and material blending. | `biome_map`, `climate_profile`, `biome_picker`. | `ALMOND_VOXEL_BIOME_TABLE` selects predefined profiles. |
| `almond_voxel/meshing/greedy_mesher.hpp` | Greedy mesher that emits indexed triangle meshes. | `greedy_mesh`, `mesh_buffer`, `mesh_config`. | `ALMOND_VOXEL_ENABLE_DECIMATION` toggles vertex deduplication. |
| `almond_voxel/serialization/region_io.hpp` | Streaming helpers for async persistence. | `region_writer`, `region_reader`, `compression_mode`. | `ALMOND_VOXEL_COMPRESSION` selects zstd/lz4/raw. |
| `almond_voxel/testing/doctest.hpp` | Thin wrapper bundling doctest defaults. | `ALMOND_VOXEL_TEST_MAIN`. | `ALMOND_VOXEL_ENABLE_ASSERTS` routes internal checks to doctest. |

All headers are aggregated through `almond_voxel/almond_voxel.hpp`, so consumers can include a single file to access the entire API surface.

## CMake integration

AlmondVoxel exports the `almond_voxel` target as an `INTERFACE` library. It is header-only and propagates required compile definitions based on the configuration flags described above.

```cmake
add_subdirectory(external/AlmondVoxel)

add_executable(worldgen main.cpp)
target_link_libraries(worldgen PRIVATE almond_voxel)

# Override defaults
set_target_properties(worldgen PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "ALMOND_VOXEL_CHUNK_SIZE=64;ALMOND_VOXEL_COMPRESSION=ZSTD"
)
```

For non-CMake build systems, add `include/` to your compiler's search path and provide the same defines manually.

## Typical usage patterns

### Chunk streaming
```cpp
#include <almond_voxel/world.hpp>
#include <almond_voxel/generation/noise.hpp>

almond::voxel::chunk_storage storage;
almond::voxel::region_manager manager{storage};

manager.ensure_region({0, 0, 0}, [&](auto& chunk) {
    almond::voxel::generate::apply_noise(
        chunk,
        almond::voxel::generate::ridged_noise{.frequency = 0.012f});
});
```

### Greedy mesh extraction
```cpp
#include <almond_voxel/meshing/greedy_mesher.hpp>

almond::voxel::mesh_config cfg;
cfg.enable_decimation = true;

almond::voxel::mesh_buffer mesh = almond::voxel::meshing::greedy_mesh(chunk, cfg);
```

### Serialization
```cpp
#include <almond_voxel/serialization/region_io.hpp>

almond::voxel::region_writer writer{"./regions"};
writer.save(manager, {0, 0, 0}, almond::voxel::compression_mode::zstd);
```

## Adding new modules

1. Place headers under an appropriate domain folder inside `include/almond_voxel/`.
2. Update the umbrella header (`almond_voxel/almond_voxel.hpp`) to export the new module.
3. Document behaviour and configuration macros in this overview.
4. Add doctest coverage under `tests/` and, if relevant, surface the feature inside one of the example applications.

## Testing helpers

The `voxel_tests` target is built with doctest and includes fixtures for chunk allocation, deterministic noise sequences, and mesh comparators. When creating new tests:

- Include `almond_voxel/testing/doctest.hpp` to pick up the shared configuration.
- Use `ALMOND_VOXEL_ENABLE_ASSERTS` to mirror runtime assertions inside tests.
- Gate experimental modules behind `ALMOND_VOXEL_EXPERIMENTAL` and document the flag here.
