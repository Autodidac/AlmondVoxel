#include "almond_voxel/editing/voxel_editing.hpp"

#include "test_framework.hpp"

using namespace almond::voxel;
using namespace almond::voxel::editing;
using namespace almond::voxel::effects;

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

TEST_CASE(editing_particle_emitter_simulation) {
    const chunk_extent extent{4, 4, 4};
    region_manager regions{extent};
    const world_position position{0, 0, 0};

    particle_emitter_brush brush{};
    brush.density = 3.5f;
    brush.lifetime = 2.5f;
    brush.initial_velocity = velocity_sample{0.25f, 1.0f, -0.5f};

    decay_settings decay{};
    decay.delta_time = 1.0f;
    decay.velocity_damping = 0.5f;

    REQUIRE(paint_particle_emitter(regions, position, brush, decay));

    const auto coords = split_world_position(position, extent);
    auto& chunk = regions.assure(coords.region);

    auto density = chunk.effect_density();
    auto lifetime = chunk.effect_lifetime();
    auto velocity = chunk.effect_velocity();

    CHECK(density(coords.local[0], coords.local[1], coords.local[2]) == brush.density);
    CHECK(lifetime(coords.local[0], coords.local[1], coords.local[2]) == brush.lifetime);
    const auto& initial_velocity = velocity(coords.local[0], coords.local[1], coords.local[2]);
    CHECK(initial_velocity.x == brush.initial_velocity.x);
    CHECK(initial_velocity.y == brush.initial_velocity.y);
    CHECK(initial_velocity.z == brush.initial_velocity.z);

    CHECK(regions.tick(1) == 1);
    CHECK(regions.tick(1) == 1);
    CHECK(regions.tick(1) == 1);
    CHECK(regions.tick(1) == 0);

    auto density_after = chunk.effect_density();
    auto lifetime_after = chunk.effect_lifetime();
    auto velocity_after = chunk.effect_velocity();

    CHECK(density_after(coords.local[0], coords.local[1], coords.local[2]) == 0.0f);
    CHECK(lifetime_after(coords.local[0], coords.local[1], coords.local[2]) == 0.0f);
    const auto& final_velocity = velocity_after(coords.local[0], coords.local[1], coords.local[2]);
    CHECK(final_velocity.x == 0.0f);
    CHECK(final_velocity.y == 0.0f);
    CHECK(final_velocity.z == 0.0f);
}
