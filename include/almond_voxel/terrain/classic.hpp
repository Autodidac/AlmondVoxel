#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/generation/noise.hpp"
#include "almond_voxel/world.hpp"

#include <cmath>

namespace almond::voxel::terrain {

struct classic_config {
    double base_height{48.0};
    double elevation_amplitude{32.0};
    double detail_amplitude{8.0};
    double base_frequency{0.008};
    double detail_frequency{0.032};
    voxel_id surface_voxel{voxel_id{1}};
    voxel_id subsurface_voxel{voxel_id{1}};
    voxel_id bedrock_voxel{voxel_id{1}};
    std::uint32_t bedrock_layers{2};
};

class classic_heightfield {
public:
    explicit classic_heightfield(chunk_extent extent = cubic_extent(32), classic_config config = {}, std::uint64_t seed = 1337);

    [[nodiscard]] chunk_extent extent() const noexcept { return extent_; }

    [[nodiscard]] chunk_storage operator()(const region_key& key) const;
    [[nodiscard]] double sample_height(double world_x, double world_y) const;
    [[nodiscard]] const classic_config& config() const noexcept { return config_; }

private:
    chunk_extent extent_{};
    classic_config config_{};
    generation::value_noise base_noise_;
    generation::value_noise detail_noise_;
};

inline classic_heightfield::classic_heightfield(chunk_extent extent, classic_config config, std::uint64_t seed)
    : extent_{extent}
    , config_{config}
    , base_noise_{seed, config.base_frequency, 5, 0.55}
    , detail_noise_{seed ^ 0xA5A5A5A5u, config.detail_frequency, 3, 0.6} {
}

inline chunk_storage classic_heightfield::operator()(const region_key& key) const {
    chunk_storage chunk{extent_};
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < extent_.z; ++z) {
        const std::int64_t world_z = static_cast<std::int64_t>(key.z) * static_cast<std::int64_t>(extent_.z)
            + static_cast<std::int64_t>(z);
        const double sample_z = static_cast<double>(world_z) + 0.5;
        for (std::uint32_t y = 0; y < extent_.y; ++y) {
            const double world_y = static_cast<double>(key.y) * static_cast<double>(extent_.y) + static_cast<double>(y);
            for (std::uint32_t x = 0; x < extent_.x; ++x) {
                const double world_x = static_cast<double>(key.x) * static_cast<double>(extent_.x) + static_cast<double>(x);
                const double height = sample_height(world_x, world_y);
                voxel_id id = voxel_id{};
                if (sample_z <= height) {
                    const double depth = height - sample_z;
                    if (depth > static_cast<double>(config_.bedrock_layers)) {
                        id = config_.subsurface_voxel;
                    } else if (depth <= 0.5) {
                        id = config_.surface_voxel;
                    } else {
                        id = config_.subsurface_voxel;
                    }
                } else if (world_z <= 0) {
                    id = config_.bedrock_voxel;
                }
                voxels(x, y, z) = id;
            }
        }
    }
    return chunk;
}

inline double classic_heightfield::sample_height(double world_x, double world_y) const {
    const double base = base_noise_.sample(world_x, world_y) * config_.elevation_amplitude;
    const double detail = detail_noise_.sample(world_x, world_y) * config_.detail_amplitude;
    return config_.base_height + base + detail;
}

} // namespace almond::voxel::terrain
