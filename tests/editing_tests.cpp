#include "almond_voxel/editing/voxel_editing.hpp"

#include "test_framework.hpp"

using namespace almond::voxel;
using namespace almond::voxel::editing;

TEST_CASE(editing_split_world_position_positive) {
    const chunk_extent extent{16, 16, 32};
    const world_position position{20, 5, 40};
    const auto coords = split_world_position(position, extent);
    CHECK(coords.region.x == 1);
    CHECK(coords.region.y == 0);
    CHECK(coords.region.z == 1);
    CHECK(coords.local[0] == 4);
    CHECK(coords.local[1] == 5);
    CHECK(coords.local[2] == 8);
}

TEST_CASE(editing_split_world_position_negative) {
    const chunk_extent extent{8, 8, 8};
    const world_position position{-1, -9, -17};
    const auto coords = split_world_position(position, extent);
    CHECK(coords.region.x == -1);
    CHECK(coords.region.y == -2);
    CHECK(coords.region.z == -3);
    CHECK(coords.local[0] == 7);
    CHECK(coords.local[1] == 7);
    CHECK(coords.local[2] == 7);
}

TEST_CASE(editing_region_manager_set_clear_toggle) {
    const chunk_extent extent{8, 8, 8};
    region_manager regions{extent};
    const world_position pos{3, -2, 6};

    CHECK(set_voxel(regions, pos, voxel_id{11}));
    const auto coords = split_world_position(pos, extent);
    auto& chunk = regions.assure(coords.region);
    auto vox = chunk.voxels();
    CHECK(vox(coords.local[0], coords.local[1], coords.local[2]) == voxel_id{11});

    CHECK(toggle_voxel(regions, pos, voxel_id{12}));
    CHECK(vox(coords.local[0], coords.local[1], coords.local[2]) == voxel_id{});

    CHECK(toggle_voxel(regions, pos, voxel_id{12}));
    CHECK(vox(coords.local[0], coords.local[1], coords.local[2]) == voxel_id{12});

    CHECK(clear_voxel(regions, pos));
    CHECK(vox(coords.local[0], coords.local[1], coords.local[2]) == voxel_id{});
}
