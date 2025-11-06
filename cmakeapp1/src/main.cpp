#include "almond_voxel/almond_voxel.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace almond::voxel;

int main() {
    chunk_storage chunk{cubic_extent(8)};
    auto voxels = chunk.voxels();
    const auto dims = chunk.extent().to_array();

    for (std::uint32_t z = 0; z < dims[2]; ++z) {
        for (std::uint32_t y = 0; y < dims[1]; ++y) {
            for (std::uint32_t x = 0; x < dims[0]; ++x) {
                voxels(x, y, z) = (x + y + z) % 3 == 0 ? static_cast<voxel_id>(2) : voxel_id{};
            }
        }
    }

    region_manager manager{chunk.extent()};
    region_key key{1, 0, 0};
    manager.assure(key).assign_voxels(chunk.voxels().linear());

    auto mesh = meshing::greedy_mesh(manager.assure(key));
    std::cout << "Generated mesh with " << mesh.vertices.size() << " vertices from "
              << chunk.volume() << " voxels\n";

    auto snapshots = manager.snapshot_loaded();
    std::vector<serialization::region_blob> blobs;
    for (const auto& snapshot : snapshots) {
        blobs.push_back(serialization::serialize_snapshot(snapshot));
    }

    std::cout << "Serialized " << blobs.size() << " region(s)\n";
    return 0;
}
