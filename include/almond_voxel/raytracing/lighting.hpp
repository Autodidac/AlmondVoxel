#pragma once

#include "almond_voxel/world.hpp"
#include "almond_voxel/raytracing/ray_queries.hpp"
#include "almond_voxel/raytracing/structures.hpp"

#include <memory>
#include <algorithm>

namespace almond::voxel::raytracing {

inline void bake_lighting(chunk_storage& chunk, const sparse_voxel_octree& svo) {
    (void)svo;
    auto voxels = chunk.voxels();
    auto blocklight = chunk.blocklight();
    auto skylight = chunk.skylight();
    if (voxels.empty() || blocklight.empty() || skylight.empty()) {
        return;
    }

    cone_trace_desc desc{};
    desc.aperture = 0.75f;
    desc.steps = 6;
    desc.max_distance = 12.0f;

    for (std::uint32_t z = 0; z < voxels.extent().z; ++z) {
        for (std::uint32_t y = 0; y < voxels.extent().y; ++y) {
            for (std::uint32_t x = 0; x < voxels.extent().x; ++x) {
                voxel_id id = voxels(x, y, z);
                if (id == voxel_id{}) {
                    blocklight(x, y, z) = 0;
                    skylight(x, y, z) = 15;
                    continue;
                }

                desc.origin = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f};
                desc.direction = {0.0f, 1.0f, 0.0f};
                float occlusion = cone_trace_occlusion(chunk, desc);
                std::uint8_t light_value = static_cast<std::uint8_t>(std::clamp(1.0f - occlusion, 0.0f, 1.0f) * 15.0f);
                blocklight(x, y, z) = light_value;
                skylight(x, y, z) = std::max<std::uint8_t>(skylight(x, y, z), light_value);
            }
        }
    }
}

inline void enqueue_global_illumination(region_manager& manager, const std::shared_ptr<acceleration_cache>& cache) {
    if (!cache) {
        return;
    }

    cache->rebuild_dirty(manager);
    manager.add_dirty_observer([cache](const region_key& key) {
        cache->invalidate_region(key);
    });

    auto snapshots = manager.snapshot_loaded(true);
    for (const auto& snapshot : snapshots) {
        if (!snapshot.chunk) {
            continue;
        }
        manager.enqueue_task(snapshot.key, [cache](chunk_storage& chunk, const region_key& key) {
            cache->update_region(key, chunk);
            if (auto* entry = cache->find(key); entry != nullptr) {
                bake_lighting(chunk, entry->svo);
                chunk.mark_dirty();
            }
        });
    }
}

} // namespace almond::voxel::raytracing

