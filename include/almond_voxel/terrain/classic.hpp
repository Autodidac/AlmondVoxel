#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/generation/noise.hpp"
#include "almond_voxel/world.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "almond_voxel/material/voxel_material.hpp"

namespace almond::voxel::terrain {

struct classic_config {
    double base_height{48.0};
    double elevation_amplitude{32.0};
    double detail_amplitude{8.0};
    double base_frequency{0.008};
    double detail_frequency{0.032};
    voxel_id surface_voxel{voxel_id{1}};
    voxel_id filler_voxel{voxel_id{1}};
    voxel_id subsurface_voxel{voxel_id{1}};
    voxel_id bedrock_voxel{voxel_id{1}};
    std::uint32_t bedrock_layers{2};
    std::uint32_t surface_depth{4};
    material_index surface_material{null_material_index};
    material_index filler_material{null_material_index};
    material_index subsurface_material{null_material_index};
    material_index bedrock_material{null_material_index};
    material_index air_material{null_material_index};
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
    chunk_storage_config chunk_config{};
    chunk_config.extent = extent_;
    chunk_config.enable_materials = true;
    chunk_storage chunk{chunk_config};
    auto voxels = chunk.voxels();
    auto materials = chunk.materials();

    const std::uint32_t size_x = extent_.x;
    const std::uint32_t size_y = extent_.y;
    const std::uint32_t size_z = extent_.z;

    const double base_world_x = static_cast<double>(key.x) * static_cast<double>(size_x);
    const double base_world_y = static_cast<double>(key.y) * static_cast<double>(size_y);
    const std::int64_t base_world_z = static_cast<std::int64_t>(key.z) * static_cast<std::int64_t>(size_z);

    std::vector<std::int32_t> column_heights(static_cast<std::size_t>(size_x) * static_cast<std::size_t>(size_y));
    for (std::uint32_t y = 0; y < size_y; ++y) {
        const double world_y = base_world_y + static_cast<double>(y);
        const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(size_x);
        for (std::uint32_t x = 0; x < size_x; ++x) {
            const double world_x = base_world_x + static_cast<double>(x);
            const double height = sample_height(world_x, world_y);
            column_heights[row_offset + x] = static_cast<std::int32_t>(std::floor(height));
        }
    }

    const std::uint32_t filler_depth = config_.surface_depth;
    const std::int64_t bedrock_limit = static_cast<std::int64_t>(config_.bedrock_layers);

    for (std::uint32_t z = 0; z < size_z; ++z) {
        const std::int64_t world_z = base_world_z + static_cast<std::int64_t>(z);
        for (std::uint32_t y = 0; y < size_y; ++y) {
            const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(size_x);
            for (std::uint32_t x = 0; x < size_x; ++x) {
                const std::int32_t column_height = column_heights[row_offset + x];
                auto& voxel = voxels(x, y, z);
                auto& material = materials(x, y, z);

                if (world_z < bedrock_limit) {
                    voxel = config_.bedrock_voxel;
                    material = config_.bedrock_material;
                    continue;
                }

                if (world_z > column_height) {
                    voxel = voxel_id{};
                    material = config_.air_material;
                    continue;
                }

                const std::int32_t depth = column_height - static_cast<std::int32_t>(world_z);
                if (depth == 0) {
                    voxel = config_.surface_voxel;
                    material = config_.surface_material;
                } else if (depth <= static_cast<std::int32_t>(filler_depth)) {
                    voxel = config_.filler_voxel;
                    material = config_.filler_material;
                } else {
                    voxel = config_.subsurface_voxel;
                    material = config_.subsurface_material;
                }
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
