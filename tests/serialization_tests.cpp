#include "almond_voxel/serialization/region_io.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <span>

using namespace almond::voxel;

TEST_CASE(chunk_serialization_roundtrip) {
    chunk_storage chunk{cubic_extent(4)};
    auto voxels = chunk.voxels();
    auto skylight = chunk.skylight();
    auto blocklight = chunk.blocklight();
    auto metadata = chunk.metadata();

    for (std::uint32_t z = 0; z < 4; ++z) {
        for (std::uint32_t y = 0; y < 4; ++y) {
            for (std::uint32_t x = 0; x < 4; ++x) {
                const auto idx = voxels.index(x, y, z);
                voxels(x, y, z) = static_cast<voxel_id>((x + 1) * (y + 2) + z);
                skylight(x, y, z) = static_cast<std::uint8_t>((idx * 3) % 16);
                blocklight(x, y, z) = static_cast<std::uint8_t>((idx * 5) % 16);
                metadata(x, y, z) = static_cast<std::uint8_t>(idx % 8);
            }
        }
    }

    const auto bytes = serialization::serialize_chunk(chunk);
    const auto restored = serialization::deserialize_chunk(std::span<const std::byte>{bytes.data(), bytes.size()});

    REQUIRE(restored.extent() == chunk.extent());

    const auto original_voxels = chunk.voxels().linear();
    const auto restored_voxels = restored.voxels().linear();
    REQUIRE(std::equal(original_voxels.begin(), original_voxels.end(), restored_voxels.begin(), restored_voxels.end()));

    const auto original_sky = chunk.skylight().linear();
    const auto restored_sky = restored.skylight().linear();
    REQUIRE(std::equal(original_sky.begin(), original_sky.end(), restored_sky.begin(), restored_sky.end()));

    const auto original_block = chunk.blocklight().linear();
    const auto restored_block = restored.blocklight().linear();
    REQUIRE(std::equal(original_block.begin(), original_block.end(), restored_block.begin(), restored_block.end()));

    const auto original_meta = chunk.metadata().linear();
    const auto restored_meta = restored.metadata().linear();
    REQUIRE(std::equal(original_meta.begin(), original_meta.end(), restored_meta.begin(), restored_meta.end()));
}
