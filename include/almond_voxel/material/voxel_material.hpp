#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace almond::voxel {

using material_index = std::uint16_t;

constexpr material_index null_material_index = material_index{0};
constexpr material_index invalid_material_index = std::numeric_limits<material_index>::max();

struct brdf_parameters {
    std::array<float, 3> base_color{1.0f, 1.0f, 1.0f};
    float roughness{0.5f};
    float metallic{0.0f};
    float specular{0.5f};
};

struct emission_properties {
    std::array<float, 3> color{0.0f, 0.0f, 0.0f};
    float intensity{0.0f};
};

struct medium_properties {
    float density{0.0f};
    std::array<float, 3> scattering{0.0f, 0.0f, 0.0f};
    std::array<float, 3> absorption{0.0f, 0.0f, 0.0f};
    float anisotropy{0.0f};
};

struct voxel_material {
    brdf_parameters brdf{};
    emission_properties emission{};
    medium_properties medium{};
};

} // namespace almond::voxel
