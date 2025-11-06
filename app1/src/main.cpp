// main.cpp
#include "almond_voxel/almond_voxel.hpp"

#include <cstdint>
#include <iostream>

using namespace almond::voxel;

int main() {
    chunk_storage chunk{cubic_extent(4)};
    auto voxels = chunk.voxels();
    const auto dims = chunk.extent().to_array();

    for (std::uint32_t z = 0; z < dims[2]; ++z) {
        for (std::uint32_t y = 0; y < dims[1]; ++y) {
            for (std::uint32_t x = 0; x < dims[0]; ++x) {
                voxels(x, y, z) = (x == 0 || y == 0 || z == 0) ? static_cast<voxel_id>(1) : voxel_id{};
            }
        }
    }

    auto mesh = meshing::greedy_mesh(chunk);
    std::cout << "Voxel boundary produced " << mesh.vertices.size() << " vertices\n";
    return 0;
}
