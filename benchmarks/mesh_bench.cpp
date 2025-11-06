#include "almond_voxel/meshing/greedy_mesher.hpp"

#include "almond_voxel/chunk.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

using namespace almond::voxel;

namespace {
void populate_sample_chunk(chunk_storage& chunk) {
    const auto size = chunk.extent().x;
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < size; ++z) {
        for (std::uint32_t y = 0; y < size; ++y) {
            for (std::uint32_t x = 0; x < size; ++x) {
                const bool filled = (x + y + z) % 3 == 0 || (z < size / 3);
                voxels(x, y, z) = filled ? voxel_id{1} : voxel_id{};
            }
        }
    }
}
}

int main() {
    constexpr std::uint32_t chunk_size = 32;
    constexpr std::size_t iterations = 64;

    chunk_storage chunk{cubic_extent(chunk_size)};
    populate_sample_chunk(chunk);

    const auto start = std::chrono::steady_clock::now();
    std::size_t total_vertices = 0;
    std::size_t total_indices = 0;
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto mesh = meshing::greedy_mesh(chunk);
        total_vertices += mesh.vertices.size();
        total_indices += mesh.indices.size();
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const double seconds = static_cast<double>(elapsed.count()) / 1000.0;
    const double meshes_per_second = seconds > 0.0 ? static_cast<double>(iterations) / seconds : 0.0;

    std::cout << "Meshed " << iterations << " chunk(s) of size " << chunk_size << '^' << 3 << " in " << seconds << "s\n";
    std::cout << "Average meshes/sec: " << meshes_per_second << "\n";
    std::cout << "Average vertices  : " << static_cast<double>(total_vertices) / iterations << "\n";
    std::cout << "Average indices   : " << static_cast<double>(total_indices) / iterations << '\n';

    return 0;
}
