#include "almond_voxel/meshing/marching_cubes.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>

int main() {
    using namespace almond::voxel;

    const chunk_extent extent = cubic_extent(32);
    const float radius = static_cast<float>(extent.x) * 0.35f;
    const float center = static_cast<float>(extent.x - 1) * 0.5f;

    auto density_sampler = [&](std::size_t x, std::size_t y, std::size_t z) {
        const float fx = static_cast<float>(x) - center;
        const float fy = static_cast<float>(y) - center;
        const float fz = static_cast<float>(z) - center;
        const float distance = std::sqrt(fx * fx + fy * fy + fz * fz);
        return radius - distance;
    };

    const meshing::marching_cubes_config config{.iso_value = 0.0f};
    const meshing::mesh_result mesh = meshing::marching_cubes(extent, density_sampler, config, voxel_id{5});

    const std::size_t triangle_count = mesh.indices.size() / 3;

    std::cout << "Extracted marching cubes surface for a sphere density field.\n";
    std::cout << "  Grid extent: " << extent.x << "x" << extent.y << "x" << extent.z << "\n";
    std::cout << "  Vertices:    " << mesh.vertices.size() << "\n";
    std::cout << "  Triangles:   " << triangle_count << "\n";

    return 0;
}
