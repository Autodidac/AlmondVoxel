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

TEST_CASE(region_manager_unpin_requeues_for_eviction) {
    region_manager regions{cubic_extent(4)};
    const region_key pinned{0, 0, 0};
    const region_key other{1, 0, 0};
    const region_key replacement{2, 0, 0};

    regions.set_max_resident(1);

    regions.assure(pinned);
    regions.pin(pinned);

    regions.assure(other);
    regions.tick(0);
    CHECK_FALSE(regions.find(other));

    regions.unpin(pinned);

    regions.assure(replacement);
    regions.tick(0);

    CHECK_FALSE(regions.find(pinned));
    CHECK(regions.find(replacement));
}
