#include "almond_voxel/chunk.hpp"
#include "test_framework.hpp"

#include <cstdint>

using namespace almond::voxel;

TEST_CASE(chunk_span_addressing) {
    const chunk_extent extent{4, 3, 2};
    chunk_storage chunk{extent};
    auto voxels = chunk.voxels();

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                const auto index = voxels.index(x, y, z);
                voxels(x, y, z) = static_cast<voxel_id>(index + 1);
                CHECK(index < voxels.size());
            }
        }
    }

    const auto flat = voxels.linear();
    for (std::size_t i = 0; i < flat.size(); ++i) {
        CHECK(flat[i] == static_cast<voxel_id>(i + 1));
    }
}
