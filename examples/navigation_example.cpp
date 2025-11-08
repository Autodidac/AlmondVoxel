#include "almond_voxel/navigation/voxel_nav.hpp"
#include "almond_voxel/world.hpp"

#include <iostream>

int main() {
    using namespace almond::voxel;

    region_manager regions{cubic_extent(8)};
    const region_key origin{0, 0, 0};

    auto& chunk = regions.assure(origin);
    auto vox = chunk.voxels();
    const auto extent = chunk.extent();
    for (std::uint32_t x = 0; x < extent.x; ++x) {
        for (std::uint32_t z = 0; z < extent.z; ++z) {
            vox(x, 0, z) = voxel_id{1};
        }
    }

    regions.enable_navigation(true);
    regions.tick();

    auto nav = regions.navigation_grid(origin);
    if (!nav) {
        std::cerr << "navigation grid not ready\n";
        return 1;
    }

    const auto start = nav->index(0, 1, 0);
    const auto goal = nav->index(extent.x - 1, 1, extent.z - 1);

    auto path = navigation::a_star(*nav, start, goal);
    if (!path) {
        std::cerr << "no path found\n";
        return 1;
    }

    std::cout << "Initial path (" << path->nodes.size() << " steps):\n";
    for (const auto node : path->nodes) {
        const auto coords = nav->coordinates(node);
        std::cout << " -> (" << coords[0] << ", " << coords[1] << ", " << coords[2] << ")\n";
    }

    const std::uint32_t barrier_x = extent.x / 2;
    auto edit = chunk.voxels();
    for (std::uint32_t z = 0; z < extent.z - 1; ++z) {
        edit(barrier_x, 1, z) = voxel_id{9};
    }

    regions.tick();

    nav = regions.navigation_grid(origin);
    if (!nav) {
        std::cerr << "navigation grid missing after edit\n";
        return 1;
    }

    auto updated_path = navigation::a_star(*nav, start, goal);
    if (!updated_path) {
        std::cout << "Path blocked after edit.\n";
        return 0;
    }

    std::cout << "Updated path (" << updated_path->nodes.size() << " steps):\n";
    for (const auto node : updated_path->nodes) {
        const auto coords = nav->coordinates(node);
        std::cout << " -> (" << coords[0] << ", " << coords[1] << ", " << coords[2] << ")\n";
    }

    const auto flow = navigation::compute_flow_field(*nav, goal);
    const auto flow_path = navigation::follow_flow(flow, start, 64);
    std::cout << "Flow-field guidance produced " << flow_path.size() << " nodes.\n";

    return 0;
}
