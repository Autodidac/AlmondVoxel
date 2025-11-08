#include "almond_voxel/raytracing/ray_queries.hpp"
#include "almond_voxel/raytracing/structures.hpp"

#include "almond_voxel/chunk.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

using namespace almond::voxel;
using namespace almond::voxel::raytracing;

namespace {
void populate(chunk_storage& chunk) {
    auto vox = chunk.voxels();
    for (std::uint32_t z = 0; z < vox.extent().z; ++z) {
        for (std::uint32_t y = 0; y < vox.extent().y; ++y) {
            for (std::uint32_t x = 0; x < vox.extent().x; ++x) {
                const bool filled = ((x * y + z) % 7) == 0 || z == 0;
                vox(x, y, z) = filled ? voxel_id{1} : voxel_id{};
            }
        }
    }
}
}

int main() {
    constexpr std::uint32_t size = 32;
    constexpr std::size_t iterations = 128;

    chunk_storage chunk{cubic_extent(size)};
    populate(chunk);

    sparse_voxel_octree tree;

    const auto build_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        tree.build(chunk);
    }
    const auto build_end = std::chrono::steady_clock::now();

    const auto build_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start);

    ray query;
    query.origin = {16.0f, 16.0f, -4.0f};
    query.direction = {0.0f, 0.0f, 1.0f};

    std::size_t hits = 0;
    const auto trace_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        auto result = trace_voxels(chunk, query, 64.0f);
        if (result.hit) {
            ++hits;
        }
    }
    const auto trace_end = std::chrono::steady_clock::now();
    const auto trace_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(trace_end - trace_start);

    std::cout << "Built " << iterations << " sparse voxel octree(s) in "
              << static_cast<double>(build_elapsed.count()) / 1000.0 << "s\n";
    std::cout << "Traced " << iterations << " ray(s) in " << static_cast<double>(trace_elapsed.count()) / 1000.0 << "s\n";
    std::cout << "Successful hits: " << hits << '\n';

    return 0;
}

