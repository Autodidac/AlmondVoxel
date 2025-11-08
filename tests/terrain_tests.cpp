#include "almond_voxel/terrain/classic.hpp"

#include "test_framework.hpp"

#include <cmath>

using namespace almond::voxel;

TEST_CASE(classic_heightfield_respects_surface_and_bedrock_layers) {
    terrain::classic_config config{};
    config.base_height = 6.0;
    config.elevation_amplitude = 0.0;
    config.detail_amplitude = 0.0;
    config.surface_voxel = voxel_id{10};
    config.filler_voxel = voxel_id{11};
    config.subsurface_voxel = voxel_id{12};
    config.bedrock_voxel = voxel_id{13};
    config.bedrock_layers = 2;
    config.surface_depth = 3;

    terrain::classic_heightfield generator{cubic_extent(8), config, 1234ull};

    const region_key origin{0, 0, 0};
    chunk_storage chunk = generator(origin);
    const auto voxels = chunk.voxels();
    const auto extent = chunk.extent();

    const std::int32_t surface_height = static_cast<std::int32_t>(
        std::floor(generator.sample_height(0.0, 0.0)));

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        const voxel_id id = voxels(0, 0, z);
        if (z < config.bedrock_layers) {
            CHECK_MESSAGE(id == config.bedrock_voxel, "bedrock layer mismatch at z=" << z);
        } else if (static_cast<std::int32_t>(z) > surface_height) {
            CHECK_MESSAGE(id == voxel_id{}, "expected air above surface at z=" << z);
        } else if (static_cast<std::int32_t>(z) == surface_height) {
            CHECK_MESSAGE(id == config.surface_voxel, "surface block mismatch at z=" << z);
        } else if (surface_height - static_cast<std::int32_t>(z)
            <= static_cast<std::int32_t>(config.surface_depth)) {
            CHECK_MESSAGE(id == config.filler_voxel, "filler block mismatch at z=" << z);
        } else {
            CHECK_MESSAGE(id == config.subsurface_voxel, "subsurface block mismatch at z=" << z);
        }
    }
}
