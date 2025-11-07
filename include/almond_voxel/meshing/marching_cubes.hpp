#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/meshing/mesh_types.hpp"
#include "almond_voxel/meshing/neighbors.hpp"
#include "almond_voxel/meshing/marching_cubes_tables.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace almond::voxel::meshing {

struct marching_cubes_config {
    // Scalar threshold for the implicit surface. Sample values strictly below the iso value are
    // classified as solid, while values greater than or equal to the iso value are treated as empty.
    float iso_value{0.5f};
};

namespace detail {

inline constexpr std::array<std::array<int, 3>, 8> cube_corners{{
    {{0, 0, 0}},
    {{1, 0, 0}},
    {{1, 1, 0}},
    {{0, 1, 0}},
    {{0, 0, 1}},
    {{1, 0, 1}},
    {{1, 1, 1}},
    {{0, 1, 1}},
}};

inline constexpr std::array<std::array<int, 2>, 12> edge_connection{{
    {{0, 1}},
    {{1, 2}},
    {{2, 3}},
    {{3, 0}},
    {{4, 5}},
    {{5, 6}},
    {{6, 7}},
    {{7, 4}},
    {{0, 4}},
    {{1, 5}},
    {{2, 6}},
    {{3, 7}},
}};

inline std::array<float, 3> interpolate_vertex(const std::array<float, 3>& p0, const std::array<float, 3>& p1,
    float v0, float v1, float iso_value) {
    const float delta = v1 - v0;
    if (std::abs(delta) < 1e-6f) {
        return p0;
    }
    const float mu = (iso_value - v0) / delta;
    return std::array<float, 3>{
        p0[0] + mu * (p1[0] - p0[0]),
        p0[1] + mu * (p1[1] - p0[1]),
        p0[2] + mu * (p1[2] - p0[2])
    };
}

inline std::array<float, 3> compute_normal(const std::array<float, 3>& p0, const std::array<float, 3>& p1,
    const std::array<float, 3>& p2) {
    const std::array<float, 3> u{
        p1[0] - p0[0],
        p1[1] - p0[1],
        p1[2] - p0[2]
    };
    const std::array<float, 3> v{
        p2[0] - p0[0],
        p2[1] - p0[1],
        p2[2] - p0[2]
    };

    std::array<float, 3> normal{
        v[1] * u[2] - v[2] * u[1],
        v[2] * u[0] - v[0] * u[2],
        v[0] * u[1] - v[1] * u[0]
    };

    const float length_sq = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
    if (length_sq <= 1e-12f) {
        return std::array<float, 3>{0.0f, 0.0f, 0.0f};
    }
    const float inv_length = 1.0f / std::sqrt(length_sq);
    normal[0] *= inv_length;
    normal[1] *= inv_length;
    normal[2] *= inv_length;
    return normal;
}

} // namespace detail

template <typename DensitySampler, typename MaterialSampler>
[[nodiscard]] mesh_result marching_cubes(chunk_extent extent, DensitySampler&& density_sampler,
    MaterialSampler&& material_sampler, const marching_cubes_config& config = {}) {
    mesh_result result;
    const std::size_t approximate_cells = static_cast<std::size_t>(extent.volume());
    result.vertices.reserve(approximate_cells * 3);
    result.indices.reserve(approximate_cells * 3);

    std::array<std::array<float, 3>, 12> edge_vertices{};
    const auto& edge_table = detail::mc_edge_table;
    const auto& triangle_table = detail::mc_triangle_table;

    for (std::size_t z = 0; z < extent.z; ++z) {
        for (std::size_t y = 0; y < extent.y; ++y) {
            for (std::size_t x = 0; x < extent.x; ++x) {
                std::array<float, 8> corner_values{};
                std::array<std::array<float, 3>, 8> corner_positions{};

                for (int corner = 0; corner < 8; ++corner) {
                    const auto& offset = detail::cube_corners[corner];
                    const std::size_t sample_x = x + static_cast<std::size_t>(offset[0]);
                    const std::size_t sample_y = y + static_cast<std::size_t>(offset[1]);
                    const std::size_t sample_z = z + static_cast<std::size_t>(offset[2]);
                    corner_values[corner] = static_cast<float>(density_sampler(sample_x, sample_y, sample_z));
                    corner_positions[corner] = std::array<float, 3>{
                        static_cast<float>(x + offset[0]),
                        static_cast<float>(y + offset[1]),
                        static_cast<float>(z + offset[2])
                    };
                }

                int cube_index = 0;
                for (int corner = 0; corner < 8; ++corner) {
                    if (corner_values[corner] < config.iso_value) {
                        cube_index |= (1 << corner);
                    }
                }

                if (edge_table[cube_index] == 0) {
                    continue;
                }

                for (int edge = 0; edge < 12; ++edge) {
                    if ((edge_table[cube_index] & (1 << edge)) == 0) {
                        continue;
                    }
                    const auto connection = detail::edge_connection[static_cast<std::size_t>(edge)];
                    edge_vertices[edge] = detail::interpolate_vertex(
                        corner_positions[connection[0]],
                        corner_positions[connection[1]],
                        corner_values[connection[0]],
                        corner_values[connection[1]],
                        config.iso_value);
                }

                const voxel_id material = material_sampler(x, y, z);
                for (int tri = 0; triangle_table[cube_index][tri] != -1; tri += 3) {
                    const int a0 = triangle_table[cube_index][tri];
                    const int a1 = triangle_table[cube_index][tri + 1];
                    const int a2 = triangle_table[cube_index][tri + 2];

                    const auto& p0 = edge_vertices[a0];
                    const auto& p1 = edge_vertices[a1];
                    const auto& p2 = edge_vertices[a2];
                    const auto normal = detail::compute_normal(p0, p1, p2);

                    const auto base_index = static_cast<std::uint32_t>(result.vertices.size());
                    result.vertices.push_back(vertex{p0, normal, {p0[0], p0[1]}, material});
                    result.vertices.push_back(vertex{p1, normal, {p1[0], p1[1]}, material});
                    result.vertices.push_back(vertex{p2, normal, {p2[0], p2[1]}, material});
                    result.indices.insert(result.indices.end(), {base_index, base_index + 1, base_index + 2});
                }
            }
        }
    }

    return result;
}

template <typename DensitySampler>
[[nodiscard]] mesh_result marching_cubes(chunk_extent extent, DensitySampler&& density_sampler,
    const marching_cubes_config& config = {}, voxel_id material = voxel_id{1}) {
    auto material_sampler = [material](std::size_t, std::size_t, std::size_t) {
        return material;
    };
    return marching_cubes(extent, std::forward<DensitySampler>(density_sampler), material_sampler, config);
}

template <typename IsSolid>
[[nodiscard]] mesh_result marching_cubes_from_chunk(const chunk_storage& chunk, IsSolid&& is_solid,
    const chunk_neighbors& neighbors, const marching_cubes_config& config = {}) {
    const auto voxels = chunk.voxels();
    const auto extent = chunk.extent();
    const auto neighbor_views = detail::load_neighbor_views(neighbors);

    auto sample_voxel = [&](std::ptrdiff_t x, std::ptrdiff_t y, std::ptrdiff_t z) -> std::optional<voxel_id> {
        if (x >= 0 && x < static_cast<std::ptrdiff_t>(extent.x) && y >= 0 && y < static_cast<std::ptrdiff_t>(extent.y)
            && z >= 0 && z < static_cast<std::ptrdiff_t>(extent.z)) {
            return voxels(static_cast<std::size_t>(x), static_cast<std::size_t>(y), static_cast<std::size_t>(z));
        }

        std::array<std::ptrdiff_t, 3> coord{x, y, z};
        const detail::neighbor_view* view = nullptr;
        if (!detail::remap_to_neighbor_coords(extent, coord, neighbor_views, view)) {
            return std::nullopt;
        }
        return view->voxels(static_cast<std::size_t>(coord[0]), static_cast<std::size_t>(coord[1]),
            static_cast<std::size_t>(coord[2]));
    };

    auto density_sampler = [&](std::size_t vx, std::size_t vy, std::size_t vz) -> float {
        const auto sample = sample_voxel(static_cast<std::ptrdiff_t>(vx), static_cast<std::ptrdiff_t>(vy),
            static_cast<std::ptrdiff_t>(vz));
        if (!sample) {
            return 1.0f;
        }
        return is_solid(*sample) ? 0.0f : 1.0f;
    };

    auto material_sampler = [&](std::size_t x, std::size_t y, std::size_t z) {
        return voxels(x, y, z);
    };

    return marching_cubes(extent, density_sampler, material_sampler, config);
}

inline mesh_result marching_cubes_from_chunk(const chunk_storage& chunk, const marching_cubes_config& config = {}) {
    return marching_cubes_from_chunk(chunk, [](voxel_id id) { return id != voxel_id{}; }, chunk_neighbors{}, config);
}

} // namespace almond::voxel::meshing
