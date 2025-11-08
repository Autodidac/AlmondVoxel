#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/core.hpp"
#include "almond_voxel/meshing/mesh_types.hpp"
#include "almond_voxel/meshing/neighbors.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace almond::voxel::meshing {

namespace detail {

struct naive_face_definition {
    std::array<std::array<float, 3>, 4> corners{};
    std::array<std::array<float, 2>, 4> uvs{};
};

constexpr std::array<naive_face_definition, block_face_count> naive_face_definitions{{
    naive_face_definition{{{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    naive_face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    naive_face_definition{{{{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    naive_face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    naive_face_definition{{{{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    naive_face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
}};

constexpr std::array<block_face, block_face_count> naive_faces{std::to_array<block_face>({
    block_face::pos_x,
    block_face::neg_x,
    block_face::pos_y,
    block_face::neg_y,
    block_face::pos_z,
    block_face::neg_z,
})};

} // namespace detail

template <typename IsOpaque, typename NeighborOpaque>
[[nodiscard]] mesh_result naive_mesh_with_neighbors(const chunk_storage& chunk, IsOpaque&& is_opaque,
    NeighborOpaque&& neighbor_opaque) {
    mesh_result result;
    const auto extent = chunk.extent();
    const auto voxels = chunk.voxels();

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                const voxel_id id = voxels(x, y, z);
                if (!is_opaque(id)) {
                    continue;
                }

                for (const block_face face : detail::naive_faces) {
                    std::array<std::ptrdiff_t, 3> neighbor_coord{
                        static_cast<std::ptrdiff_t>(x),
                        static_cast<std::ptrdiff_t>(y),
                        static_cast<std::ptrdiff_t>(z),
                    };
                    const auto normal_i = face_normal(face);
                    neighbor_coord[0] += normal_i[0];
                    neighbor_coord[1] += normal_i[1];
                    neighbor_coord[2] += normal_i[2];

                    bool neighbor_solid = false;
                    const bool neighbor_inside = neighbor_coord[0] >= 0
                        && neighbor_coord[0] < static_cast<std::ptrdiff_t>(extent.x)
                        && neighbor_coord[1] >= 0
                        && neighbor_coord[1] < static_cast<std::ptrdiff_t>(extent.y)
                        && neighbor_coord[2] >= 0
                        && neighbor_coord[2] < static_cast<std::ptrdiff_t>(extent.z);
                    if (neighbor_inside) {
                        neighbor_solid = is_opaque(voxels(static_cast<std::size_t>(neighbor_coord[0]),
                            static_cast<std::size_t>(neighbor_coord[1]), static_cast<std::size_t>(neighbor_coord[2])));
                    } else {
                        neighbor_solid = neighbor_opaque(neighbor_coord);
                    }

                    if (neighbor_solid) {
                        continue;
                    }

                    const auto& definition = detail::naive_face_definitions[static_cast<std::size_t>(face)];
                    const std::array<float, 3> base{
                        static_cast<float>(x),
                        static_cast<float>(y),
                        static_cast<float>(z),
                    };

                    const std::array<float, 3> normal{
                        static_cast<float>(normal_i[0]),
                        static_cast<float>(normal_i[1]),
                        static_cast<float>(normal_i[2]),
                    };

                    const auto base_index = static_cast<std::uint32_t>(result.vertices.size());
                    for (std::size_t i = 0; i < definition.corners.size(); ++i) {
                        vertex v{};
                        v.position = {
                            base[0] + definition.corners[i][0],
                            base[1] + definition.corners[i][1],
                            base[2] + definition.corners[i][2],
                        };
                        v.normal = normal;
                        v.uv = definition.uvs[i];
                        v.id = id;
                        result.vertices.push_back(v);
                    }

                    result.indices.insert(result.indices.end(),
                        {base_index, base_index + 1, base_index + 2, base_index, base_index + 2, base_index + 3});
                }
            }
        }
    }

    return result;
}

template <typename IsOpaque>
[[nodiscard]] mesh_result naive_mesh_with_neighbor_chunks(const chunk_storage& chunk, const chunk_neighbors& neighbors,
    IsOpaque&& is_opaque) {
    const auto neighbor_views = detail::load_neighbor_views(neighbors);
    auto neighbor_sampler = [&, dims = chunk.extent()](const std::array<std::ptrdiff_t, 3>& coord) {
        std::array<std::ptrdiff_t, 3> local = coord;
        const detail::neighbor_view* view = nullptr;
        if (!detail::remap_to_neighbor_coords(dims, local, neighbor_views, view)) {
            return false;
        }

        return is_opaque(view->voxels(static_cast<std::size_t>(local[0]), static_cast<std::size_t>(local[1]),
            static_cast<std::size_t>(local[2])));
    };

    return naive_mesh_with_neighbors(chunk, std::forward<IsOpaque>(is_opaque), neighbor_sampler);
}

inline mesh_result naive_mesh_with_neighbor_chunks(const chunk_storage& chunk, const chunk_neighbors& neighbors) {
    return naive_mesh_with_neighbor_chunks(chunk, neighbors, [](voxel_id id) { return id != voxel_id{}; });
}

template <typename IsOpaque>
[[nodiscard]] mesh_result naive_mesh(const chunk_storage& chunk, IsOpaque&& is_opaque) {
    auto neighbor = [](const std::array<std::ptrdiff_t, 3>&) { return false; };
    return naive_mesh_with_neighbors(chunk, std::forward<IsOpaque>(is_opaque), neighbor);
}

inline mesh_result naive_mesh(const chunk_storage& chunk) {
    return naive_mesh(chunk, [](voxel_id id) { return id != voxel_id{}; });
}

} // namespace almond::voxel::meshing

