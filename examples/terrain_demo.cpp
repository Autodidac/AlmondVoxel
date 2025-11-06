#include "almond_voxel/world.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {
using namespace almond::voxel;

void generate_heightmap(chunk_storage& chunk) {
    const auto extent = chunk.extent();
    auto voxels = chunk.voxels();
    const float scale_x = 2.0f / static_cast<float>(extent.x);
    const float scale_y = 2.0f / static_cast<float>(extent.y);

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                voxels(x, y, z) = voxel_id{};
            }
        }
    }

    for (std::uint32_t y = 0; y < extent.y; ++y) {
        for (std::uint32_t x = 0; x < extent.x; ++x) {
            const float fx = static_cast<float>(x) * scale_x - 1.0f;
            const float fy = static_cast<float>(y) * scale_y - 1.0f;
            const float height = (std::sin(fx * 3.14159f) + std::cos(fy * 3.14159f)) * 0.25f + 0.5f;
            const std::uint32_t max_z = static_cast<std::uint32_t>(std::clamp(height * static_cast<float>(extent.z), 0.0f,
                static_cast<float>(extent.z)));
            for (std::uint32_t z = 0; z < max_z && z < extent.z; ++z) {
                voxels(x, y, z) = voxel_id{1};
            }
        }
    }
}

void print_statistics(const meshing::mesh_result& mesh) {
    std::uint64_t checksum = 0;
    for (const auto& vertex : mesh.vertices) {
        checksum += static_cast<std::uint64_t>(vertex.position[0] * 17.0f + vertex.position[1] * 31.0f + vertex.position[2] * 47.0f)
            + vertex.id;
    }
    std::cout << "Terrain demo results\n";
    std::cout << "  Vertices : " << mesh.vertices.size() << "\n";
    std::cout << "  Indices  : " << mesh.indices.size() << "\n";
    std::cout << "  Checksum : 0x" << std::hex << checksum << std::dec << "\n";
}
} // namespace

int main() {
    using namespace almond::voxel;

    region_manager world{cubic_extent(32)};
    const region_key key{0, 0, 0};
    auto& chunk = world.assure(key);

    chunk.fill(voxel_id{});
    generate_heightmap(chunk);

    const auto mesh = meshing::greedy_mesh(chunk);
    print_statistics(mesh);

    return 0;
}
