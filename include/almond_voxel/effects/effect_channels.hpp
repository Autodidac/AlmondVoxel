#pragma once

#include "almond_voxel/core.hpp"

#include <array>
#include <cstdint>

namespace almond::voxel::effects {

enum class channel : std::uint32_t {
    none = 0u,
    density = 1u << 0u,
    velocity = 1u << 1u,
    lifetime = 1u << 2u,
    all = density | velocity | lifetime
};

constexpr channel operator|(channel lhs, channel rhs) noexcept {
    return static_cast<channel>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr channel operator&(channel lhs, channel rhs) noexcept {
    return static_cast<channel>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr channel operator~(channel value) noexcept {
    return static_cast<channel>(~static_cast<std::uint32_t>(value));
}

constexpr channel& operator|=(channel& lhs, channel rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool contains(channel flags, channel value) noexcept {
    return (flags & value) != channel::none;
}

struct velocity_sample {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr std::array<float, 3> to_array() const noexcept { return {x, y, z}; }
};

} // namespace almond::voxel::effects

