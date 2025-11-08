#pragma once

#include <cstdint>

namespace almond::voxel {

struct region_key {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};

    [[nodiscard]] friend constexpr bool operator==(const region_key& lhs, const region_key& rhs) noexcept = default;
};

struct region_key_hash {
    [[nodiscard]] std::size_t operator()(const region_key& key) const noexcept {
        std::uint64_t hx = static_cast<std::uint64_t>(key.x);
        std::uint64_t hy = static_cast<std::uint64_t>(key.y);
        std::uint64_t hz = static_cast<std::uint64_t>(key.z);
        std::uint64_t hash = hx * 0x9E3779B185EBCA87ull;
        hash ^= hy + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
        hash ^= hz + 0xC2B2AE3D27D4EB4Full + (hash << 6) + (hash >> 2);
        return static_cast<std::size_t>(hash);
    }
};

} // namespace almond::voxel
