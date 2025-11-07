#pragma once

#include "almond_voxel/core.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace almond::voxel::meshing {

struct vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    voxel_id id{0};
};

struct mesh_result {
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace almond::voxel::meshing
