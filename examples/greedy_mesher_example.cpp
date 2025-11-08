#include "almond_voxel/chunk.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"

#include <cstdint>
#include <iostream>

int main() {
    using namespace almond::voxel;

    chunk_storage chunk{cubic_extent(32)};
    auto voxels = chunk.voxels();

    for (std::uint32_t z = 0; z < chunk.extent().z; ++z) {
        for (std::uint32_t y = 0; y < chunk.extent().y; ++y) {
            for (std::uint32_t x = 0; x < chunk.extent().x; ++x) {
                if (z < 8 || (x + y + z) % 11 == 0) {
                    voxels(x, y, z) = voxel_id{1};
                } else {
                    voxels(x, y, z) = voxel_id{};
                }
            }
        }
    }

    const meshing::mesh_result mesh = meshing::greedy_mesh(chunk);

    const std::size_t triangle_count = mesh.indices.size() / 3;

    std::cout << "Generated greedy mesh from chunk of extent "
              << chunk.extent().x << "x" << chunk.extent().y << "x" << chunk.extent().z << "\n";
    std::cout << "  Vertices:  " << mesh.vertices.size() << "\n";
    std::cout << "  Triangles: " << triangle_count << "\n";

    return 0;
}
