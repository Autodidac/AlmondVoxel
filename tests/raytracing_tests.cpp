#include "almond_voxel/raytracing/lighting.hpp"
#include "almond_voxel/raytracing/ray_queries.hpp"
#include "almond_voxel/raytracing/structures.hpp"
#include "test_framework.hpp"

#include <memory>

using namespace almond::voxel;
using namespace almond::voxel::raytracing;

TEST_CASE(raytracing_octree_captures_bounds) {
    chunk_storage chunk{cubic_extent(4)};
    chunk.fill(voxel_id{});
    auto vox = chunk.voxels();
    vox(1, 1, 1) = voxel_id{7};

    sparse_voxel_octree tree;
    tree.build(chunk, 2);

    CHECK(!tree.nodes().empty());
    const auto& root = tree.root();
    CHECK(root.bounds.occupied);
    CHECK(root.bounds.min_material == voxel_id{7});
    CHECK(root.bounds.max_material == voxel_id{7});
}

TEST_CASE(raytracing_ray_query_hits_voxel) {
    chunk_storage chunk{cubic_extent(8)};
    chunk.fill(voxel_id{});
    auto vox = chunk.voxels();
    vox(3, 3, 3) = voxel_id{5};

    ray r;
    r.origin = {3.5f, 3.5f, 0.0f};
    r.direction = {0.0f, 0.0f, 1.0f};

    auto hit = trace_voxels(chunk, r, 10.0f);
    CHECK(hit.hit);
    CHECK(hit.material == voxel_id{5});
    CHECK(hit.position[2] == 3);
}

TEST_CASE(raytracing_lighting_updates_on_dirty) {
    region_manager manager{cubic_extent(4)};
    auto cache = std::make_shared<acceleration_cache>();

    region_key key{0, 0, 0};
    auto& chunk = manager.assure(key);
    chunk.fill(voxel_id{});
    auto vox = chunk.voxels();
    vox(1, 1, 1) = voxel_id{2};

    bool observed = false;
    manager.add_dirty_observer([&](const region_key& notified) {
        if (notified == key) {
            observed = true;
            cache->invalidate_region(notified);
        }
    });

    cache->update_region(key, chunk);
    auto* entry = cache->find(key);
    CHECK(entry != nullptr);
    bake_lighting(chunk, entry->svo);

    vox(1, 1, 1) = voxel_id{3};
    chunk.mark_dirty();
    CHECK(observed);

    cache->rebuild_dirty(manager);
    auto* refreshed = cache->find(key);
    CHECK(refreshed != nullptr);
    CHECK(refreshed->svo.root().bounds.max_material == voxel_id{3});
}

