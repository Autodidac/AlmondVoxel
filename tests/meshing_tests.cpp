#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "test_framework.hpp"

#include "almond_voxel/chunk.hpp"

#include <cstddef>

using namespace almond::voxel;

TEST_CASE(greedy_mesher_single_voxel) {
    chunk_storage chunk{cubic_extent(3)};
    chunk.fill(voxel_id{});
    auto voxels = chunk.voxels();
    voxels(1, 1, 1) = voxel_id{42};

    const auto mesh = meshing::greedy_mesh(chunk);
    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);

    for (const auto index : mesh.indices) {
        CHECK(index < mesh.vertices.size());
    }
}
