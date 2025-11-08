#include "almond_voxel/serialization/region_io.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

using namespace almond::voxel;
using namespace almond::voxel::effects;

TEST_CASE(chunk_serialization_roundtrip) {
    chunk_storage_config config{};
    config.extent = cubic_extent(4);
    config.enable_materials = true;
    config.enable_high_precision_lighting = true;
    config.effect_channels = effects::channel::density | effects::channel::velocity | effects::channel::lifetime;
    chunk_storage chunk{config};
    auto voxels = chunk.voxels();
    auto skylight = chunk.skylight();
    auto blocklight = chunk.blocklight();
    auto metadata = chunk.metadata();
    auto materials = chunk.materials();
    auto sky_cache = chunk.skylight_cache();
    auto block_cache = chunk.blocklight_cache();
    auto effect_density = chunk.effect_density();
    auto effect_velocity = chunk.effect_velocity();
    auto effect_lifetime = chunk.effect_lifetime();

    for (std::uint32_t z = 0; z < 4; ++z) {
        for (std::uint32_t y = 0; y < 4; ++y) {
            for (std::uint32_t x = 0; x < 4; ++x) {
                const auto idx = voxels.index(x, y, z);
                voxels(x, y, z) = static_cast<voxel_id>((x + 1) * (y + 2) + z);
                skylight(x, y, z) = static_cast<std::uint8_t>((idx * 3) % 16);
                blocklight(x, y, z) = static_cast<std::uint8_t>((idx * 5) % 16);
                metadata(x, y, z) = static_cast<std::uint8_t>(idx % 8);
                materials(x, y, z) = static_cast<material_index>((idx % 7) + 1);
                sky_cache(x, y, z) = static_cast<float>(idx) * 0.125f;
                block_cache(x, y, z) = static_cast<float>(idx) * 0.0625f;
                effect_density(x, y, z) = static_cast<float>(idx) * 0.5f;
                effect_velocity(x, y, z) = effects::velocity_sample{
                    static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                effect_lifetime(x, y, z) = 5.0f - static_cast<float>(idx) * 0.1f;
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

    const auto original_materials = chunk.materials().linear();
    const auto restored_materials = restored.materials().linear();
    REQUIRE(std::equal(original_materials.begin(), original_materials.end(), restored_materials.begin(),
        restored_materials.end()));

    const auto original_sky_cache = chunk.skylight_cache().linear();
    const auto restored_sky_cache = restored.skylight_cache().linear();
    REQUIRE(std::equal(original_sky_cache.begin(), original_sky_cache.end(), restored_sky_cache.begin(),
        restored_sky_cache.end()));

    const auto original_block_cache = chunk.blocklight_cache().linear();
    const auto restored_block_cache = restored.blocklight_cache().linear();
    REQUIRE(std::equal(original_block_cache.begin(), original_block_cache.end(), restored_block_cache.begin(),
        restored_block_cache.end()));

    const auto original_effect_density = chunk.effect_density().linear();
    const auto restored_effect_density = restored.effect_density().linear();
    REQUIRE(std::equal(original_effect_density.begin(), original_effect_density.end(),
        restored_effect_density.begin(), restored_effect_density.end()));

    const auto original_effect_velocity = chunk.effect_velocity().linear();
    const auto restored_effect_velocity = restored.effect_velocity().linear();
    REQUIRE(std::equal(original_effect_velocity.begin(), original_effect_velocity.end(),
        restored_effect_velocity.begin(), restored_effect_velocity.end(),
        [](const effects::velocity_sample& lhs, const effects::velocity_sample& rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }));

    const auto original_effect_lifetime = chunk.effect_lifetime().linear();
    const auto restored_effect_lifetime = restored.effect_lifetime().linear();
    REQUIRE(std::equal(original_effect_lifetime.begin(), original_effect_lifetime.end(),
        restored_effect_lifetime.begin(), restored_effect_lifetime.end()));
}

TEST_CASE(chunk_legacy_payload_migration) {
    chunk_storage legacy_chunk{cubic_extent(2)};
    auto voxels = legacy_chunk.voxels();
    auto skylight = legacy_chunk.skylight();
    auto blocklight = legacy_chunk.blocklight();
    auto metadata = legacy_chunk.metadata();

    for (std::uint32_t z = 0; z < 2; ++z) {
        for (std::uint32_t y = 0; y < 2; ++y) {
            for (std::uint32_t x = 0; x < 2; ++x) {
                const auto idx = voxels.index(x, y, z);
                voxels(x, y, z) = static_cast<voxel_id>(idx + 5);
                skylight(x, y, z) = static_cast<std::uint8_t>(idx + 1);
                blocklight(x, y, z) = static_cast<std::uint8_t>(idx + 2);
                metadata(x, y, z) = static_cast<std::uint8_t>(idx + 3);
            }
        }
    }

    serialization::chunk_header_v1 header{};
    header.extent[0] = 2;
    header.extent[1] = 2;
    header.extent[2] = 2;
    const std::size_t count = legacy_chunk.extent().volume();
    std::vector<std::byte> legacy_payload(sizeof(header) + count * (sizeof(voxel_id) + 3));
    std::memcpy(legacy_payload.data(), &header, sizeof(header));
    auto* ptr = legacy_payload.data() + sizeof(header);
    auto copy_linear = [&ptr, count](auto span) {
        using value_type = typename decltype(span)::value_type;
        std::memcpy(ptr, span.data(), count * sizeof(value_type));
        ptr += count * sizeof(value_type);
    };
    copy_linear(voxels.linear());
    copy_linear(skylight.linear());
    copy_linear(blocklight.linear());
    copy_linear(metadata.linear());

    REQUIRE(serialization::is_legacy_chunk_payload(legacy_payload));
    const auto migrated = serialization::migrate_legacy_chunk_payload(legacy_payload);
    REQUIRE_FALSE(serialization::is_legacy_chunk_payload(migrated));

    const auto restored = serialization::deserialize_chunk(migrated);
    REQUIRE(restored.extent() == legacy_chunk.extent());
    REQUIRE(std::equal(legacy_chunk.voxels().linear().begin(), legacy_chunk.voxels().linear().end(),
        restored.voxels().linear().begin(), restored.voxels().linear().end()));
    REQUIRE(std::equal(legacy_chunk.skylight().linear().begin(), legacy_chunk.skylight().linear().end(),
        restored.skylight().linear().begin(), restored.skylight().linear().end()));
    REQUIRE(std::equal(legacy_chunk.blocklight().linear().begin(), legacy_chunk.blocklight().linear().end(),
        restored.blocklight().linear().begin(), restored.blocklight().linear().end()));
    REQUIRE(std::equal(legacy_chunk.metadata().linear().begin(), legacy_chunk.metadata().linear().end(),
        restored.metadata().linear().begin(), restored.metadata().linear().end()));
}
