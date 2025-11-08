#include "almond_voxel/navigation/voxel_nav.hpp"
#include "almond_voxel/world.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <array>

using namespace almond::voxel;

TEST_CASE(navigation_grid_pathing_handles_obstacles) {
    chunk_storage chunk{cubic_extent(6)};
    auto vox = chunk.voxels();
    const auto extent = chunk.extent();
    for (std::uint32_t x = 0; x < extent.x; ++x) {
        for (std::uint32_t z = 0; z < extent.z; ++z) {
            vox(x, 0, z) = voxel_id{1};
        }
    }
    vox(2, 1, 2) = voxel_id{5};

    navigation::nav_build_config config;
    config.clearance = 2;

    const auto grid = navigation::build_nav_grid(chunk, config);
    const auto start = grid.index(0, 1, 0);
    const auto goal = grid.index(extent.x - 1, 1, extent.z - 1);

    auto path = navigation::a_star(grid, start, goal);
    REQUIRE(path.has_value());
    CHECK(path->nodes.front() == start);
    CHECK(path->nodes.back() == goal);

    const auto flow = navigation::compute_flow_field(grid, goal);
    const auto flow_path = navigation::follow_flow(flow, start, 64);
    REQUIRE_FALSE(flow_path.empty());
    CHECK(flow_path.front() == start);
    CHECK(flow_path.back() == goal);
}

TEST_CASE(region_manager_navigation_rebuild_after_edit) {
    region_manager regions{cubic_extent(4)};
    const region_key key{0, 0, 0};

    auto& chunk = regions.assure(key);
    auto vox = chunk.voxels();
    const auto extent = chunk.extent();
    for (std::uint32_t x = 0; x < extent.x; ++x) {
        for (std::uint32_t z = 0; z < extent.z; ++z) {
            vox(x, 0, z) = voxel_id{1};
        }
    }

    regions.enable_navigation(true);
    regions.tick();

    auto nav = regions.navigation_grid(key);
    REQUIRE(nav);
    const auto start = nav->index(0, 1, 0);
    const auto goal = nav->index(extent.x - 1, 1, extent.z - 1);
    const auto obstacle_idx = nav->index(extent.x / 2, 1, extent.z / 2);
    CHECK(nav->walkable(obstacle_idx));

    auto edit = chunk.voxels();
    edit(extent.x / 2, 1, extent.z / 2) = voxel_id{7};

    regions.tick();

    nav = regions.navigation_grid(key);
    REQUIRE(nav);
    CHECK_FALSE(nav->walkable(obstacle_idx));

    auto new_path = navigation::a_star(*nav, start, goal);
    REQUIRE(new_path);
    CHECK(new_path->nodes.front() == start);
    CHECK(new_path->nodes.back() == goal);
}

TEST_CASE(navigation_stitched_graph_links_neighbors) {
    region_manager regions{cubic_extent(4)};
    const region_key base{0, 0, 0};
    const region_key neighbor{1, 0, 0};

    auto& base_chunk = regions.assure(base);
    auto base_vox = base_chunk.voxels();
    auto& neighbor_chunk = regions.assure(neighbor);
    auto neighbor_vox = neighbor_chunk.voxels();

    const auto extent = base_chunk.extent();
    for (std::uint32_t x = 0; x < extent.x; ++x) {
        for (std::uint32_t z = 0; z < extent.z; ++z) {
            base_vox(x, 0, z) = voxel_id{1};
            neighbor_vox(x, 0, z) = voxel_id{1};
        }
    }

    regions.enable_navigation(true);
    regions.tick();

    auto nav_base = regions.navigation_grid(base);
    auto nav_neighbor = regions.navigation_grid(neighbor);
    REQUIRE(nav_base);
    REQUIRE(nav_neighbor);

    const std::array<region_key, 1> neighbors{neighbor};
    auto stitched = regions.stitch_navigation(base, neighbors);
    CHECK(stitched.regions.size() >= 2);
    CHECK_FALSE(stitched.bridges.empty());

    const bool has_forward = std::any_of(stitched.bridges.begin(), stitched.bridges.end(), [&](const navigation::nav_bridge& bridge) {
        return bridge.from_region == base && bridge.to_region == neighbor;
    });
    const bool has_reverse = std::any_of(stitched.bridges.begin(), stitched.bridges.end(), [&](const navigation::nav_bridge& bridge) {
        return bridge.from_region == neighbor && bridge.to_region == base;
    });
    CHECK(has_forward);
    CHECK(has_reverse);
}
