#include "almond_voxel/almond_voxel.hpp"

#include <cstdint>
#include <iostream>

using namespace almond::voxel;

namespace {
void populate_surface(chunk_storage& chunk) {
    const auto dims = chunk.extent().to_array();
    generation::value_noise noise{1337u, 1.5, 4, 0.55};
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < dims[2]; ++z) {
        for (std::uint32_t x = 0; x < dims[0]; ++x) {
            const double nx = static_cast<double>(x) / static_cast<double>(dims[0]);
            const double nz = static_cast<double>(z) / static_cast<double>(dims[2]);
            const double height = (noise.sample(nx, nz) * 0.5 + 0.5) * static_cast<double>(dims[1] - 1);
            const auto cutoff = static_cast<std::uint32_t>(height);
            for (std::uint32_t y = 0; y < dims[1]; ++y) {
                voxels(x, y, z) = y <= cutoff ? static_cast<voxel_id>(1) : voxel_id{};
            }
        }
    }
}
}

int main() {
    chunk_storage chunk{cubic_extent(16)};
    populate_surface(chunk);

    region_manager manager{chunk.extent()};
    region_key key{0, 0, 0};
    auto& stored = manager.assure(key);
    stored.assign_voxels(chunk.voxels().linear());

    manager.enqueue_task(key, [](chunk_storage& c, const region_key&) {
        c.request_compression();
        c.flush_compression();
    });
    manager.tick();

    auto mesh = meshing::greedy_mesh(stored);
    std::cout << "Chunk volume: " << stored.volume() << " voxels\n";
    std::cout << "Greedy mesh emitted " << mesh.vertices.size() << " vertices and "
              << mesh.indices.size() / 3 << " triangles\n";

    auto snapshots = manager.snapshot_loaded(true);
    if (!snapshots.empty()) {
        serialization::region_blob blob = serialization::serialize_snapshot(snapshots.front());
        serialization::ingest_blob(manager, blob);
    }

    return 0;
}
