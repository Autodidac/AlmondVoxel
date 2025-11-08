#include "almond_voxel/world.hpp"

#include "test_framework.hpp"

using namespace almond::voxel;

TEST_CASE(region_manager_readonly_task_keeps_chunk_clean) {
    const region_key key{0, 0, 0};
    region_manager regions{cubic_extent(4)};
    auto& chunk = regions.assure(key);
    chunk.mark_dirty(false);
    CHECK_FALSE(chunk.dirty());

    regions.enqueue_task(key, [](chunk_storage& chunk_ref, const region_key&) {
        const auto& const_chunk = static_cast<const chunk_storage&>(chunk_ref);
        const auto voxels = const_chunk.voxels();
        CHECK(voxels.contains(0, 0, 0));
    });

    const std::size_t processed = regions.tick(1);
    CHECK(processed == 1);
    CHECK_FALSE(chunk.dirty());
}
