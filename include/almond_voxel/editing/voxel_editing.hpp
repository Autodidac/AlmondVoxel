#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/world.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace almond::voxel::editing {

struct world_position {
    std::int64_t x{0};
    std::int64_t y{0};
    std::int64_t z{0};
};

struct chunk_coordinates {
    region_key region{};
    std::array<std::uint32_t, 3> local{};
};

namespace detail {

inline std::pair<std::int32_t, std::uint32_t> floor_divmod(std::int64_t value, std::uint32_t divisor) {
    const std::int64_t denom = static_cast<std::int64_t>(divisor);
    std::int64_t quotient = value / denom;
    std::int64_t remainder = value % denom;
    if (remainder < 0) {
        remainder += denom;
        --quotient;
    }
    return {static_cast<std::int32_t>(quotient), static_cast<std::uint32_t>(remainder)};
}

} // namespace detail

inline chunk_coordinates split_world_position(const world_position& position, const chunk_extent& extent) {
    auto [rx, lx] = detail::floor_divmod(position.x, extent.x);
    auto [ry, ly] = detail::floor_divmod(position.y, extent.y);
    auto [rz, lz] = detail::floor_divmod(position.z, extent.z);
    chunk_coordinates coords{};
    coords.region = region_key{rx, ry, rz};
    coords.local = {lx, ly, lz};
    return coords;
}

inline std::size_t linear_index(const chunk_extent& extent, const std::array<std::uint32_t, 3>& local) {
    return static_cast<std::size_t>(local[0])
        + static_cast<std::size_t>(extent.x) * (static_cast<std::size_t>(local[1])
              + static_cast<std::size_t>(extent.y) * static_cast<std::size_t>(local[2]));
}

inline bool set_voxel(chunk_storage& chunk, const std::array<std::uint32_t, 3>& local, voxel_id id) {
    auto vox = chunk.voxels();
    if (!vox.contains(local[0], local[1], local[2])) {
        return false;
    }
    vox(local[0], local[1], local[2]) = id;
    return true;
}

inline bool clear_voxel(chunk_storage& chunk, const std::array<std::uint32_t, 3>& local) {
    return set_voxel(chunk, local, voxel_id{});
}

inline bool set_voxel(region_manager& regions, const world_position& position, voxel_id id) {
    const auto coords = split_world_position(position, regions.chunk_dimensions());
    auto& chunk = regions.assure(coords.region);
    return set_voxel(chunk, coords.local, id);
}

inline bool clear_voxel(region_manager& regions, const world_position& position) {
    return set_voxel(regions, position, voxel_id{});
}

inline bool toggle_voxel(region_manager& regions, const world_position& position, voxel_id on_value) {
    const auto coords = split_world_position(position, regions.chunk_dimensions());
    auto& chunk = regions.assure(coords.region);
    auto vox = chunk.voxels();
    if (!vox.contains(coords.local[0], coords.local[1], coords.local[2])) {
        return false;
    }
    voxel_id& value = vox(coords.local[0], coords.local[1], coords.local[2]);
    value = value == voxel_id{} ? on_value : voxel_id{};
    return true;
}

} // namespace almond::voxel::editing
