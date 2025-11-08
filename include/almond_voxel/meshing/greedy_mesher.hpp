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

template <typename IsOpaque, typename NeighborOpaque>
[[nodiscard]] mesh_result greedy_mesh_with_neighbors(const chunk_storage& chunk, IsOpaque&& is_opaque,
    NeighborOpaque&& neighbor_opaque) {
    mesh_result result;
    const auto extent = chunk.extent();
    const auto dims = extent.to_array();
    const auto voxels = chunk.voxels();

    struct mask_cell {
        bool filled{false};
        voxel_id id{0};
    };

    const std::array faces{block_face::pos_x, block_face::neg_x, block_face::pos_y, block_face::neg_y, block_face::pos_z, block_face::neg_z};
    constexpr float vertical_face_bias = 0.001f;

    for (auto face : faces) {
        const std::size_t axis = static_cast<std::size_t>(axis_of(face));
        const int sign = axis_sign(face);
        const std::size_t u_axis = (axis + 1) % 3;
        const std::size_t v_axis = (axis + 2) % 3;
        const std::size_t du = dims[u_axis];
        const std::size_t dv = dims[v_axis];

        std::vector<mask_cell> mask(du * dv);

        for (std::size_t plane = 0; plane < dims[axis]; ++plane) {
            std::fill(mask.begin(), mask.end(), mask_cell{});

            for (std::size_t v = 0; v < dv; ++v) {
                for (std::size_t u = 0; u < du; ++u) {
                    const std::size_t idx = u + v * du;
                    std::array<std::size_t, 3> pos{};
                    pos[axis] = plane;
                    pos[u_axis] = u;
                    pos[v_axis] = v;

                    const voxel_id current = voxels(pos[0], pos[1], pos[2]);
                    if (!is_opaque(current)) {
                        continue;
                    }

                    bool neighbor_inside = true;
                    std::array<std::size_t, 3> neighbor = pos;
                    if (sign > 0) {
                        neighbor[axis] = pos[axis] + 1;
                        neighbor_inside = neighbor[axis] < dims[axis];
                    } else {
                        neighbor_inside = pos[axis] > 0;
                        if (neighbor_inside) {
                            neighbor[axis] = pos[axis] - 1;
                        }
                    }

                    bool neighbor_solid = false;
                    if (neighbor_inside) {
                        neighbor_solid = is_opaque(voxels(neighbor[0], neighbor[1], neighbor[2]));
                    } else {
                        std::array<std::ptrdiff_t, 3> neighbor_local{
                            static_cast<std::ptrdiff_t>(pos[0]),
                            static_cast<std::ptrdiff_t>(pos[1]),
                            static_cast<std::ptrdiff_t>(pos[2])
                        };
                        neighbor_local[axis] += sign;
                        neighbor_solid = neighbor_opaque(neighbor_local);
                    }

                    if (!neighbor_solid) {
                        mask[idx] = mask_cell{true, current};
                    }
                }
            }

            for (std::size_t v = 0; v < dv; ++v) {
                for (std::size_t u = 0; u < du; ++u) {
                    const std::size_t idx = u + v * du;
                    auto& cell = mask[idx];
                    if (!cell.filled) {
                        continue;
                    }

                    std::size_t width = 1;
                    while (u + width < du) {
                        const auto& next = mask[idx + width];
                        if (!next.filled || next.id != cell.id) {
                            break;
                        }
                        ++width;
                    }

                    std::size_t height = 1;
                    bool stop = false;
                    while (v + height < dv && !stop) {
                        for (std::size_t x = 0; x < width; ++x) {
                            const auto& next = mask[idx + x + height * du];
                            if (!next.filled || next.id != cell.id) {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop) {
                            ++height;
                        }
                    }

                    float axis_coord = static_cast<float>(plane + (sign > 0 ? 1 : 0));
                    if (axis == 2) {
                        axis_coord += sign > 0 ? vertical_face_bias : -vertical_face_bias;
                    }
                    std::array<float, 3> base{0.0f, 0.0f, 0.0f};
                    base[axis] = axis_coord;
                    base[u_axis] = static_cast<float>(u);
                    base[v_axis] = static_cast<float>(v);

                    std::array<float, 3> du_vec{0.0f, 0.0f, 0.0f};
                    du_vec[u_axis] = static_cast<float>(width);
                    std::array<float, 3> dv_vec{0.0f, 0.0f, 0.0f};
                    dv_vec[v_axis] = static_cast<float>(height);

                    std::array<std::array<float, 3>, 4> corners{
                        base,
                        std::array<float, 3>{base[0] + du_vec[0], base[1] + du_vec[1], base[2] + du_vec[2]},
                        std::array<float, 3>{base[0] + du_vec[0] + dv_vec[0], base[1] + du_vec[1] + dv_vec[1], base[2] + du_vec[2] + dv_vec[2]},
                        std::array<float, 3>{base[0] + dv_vec[0], base[1] + dv_vec[1], base[2] + dv_vec[2]}
                    };

                    std::array<std::array<float, 2>, 4> uv{
                        std::array<float, 2>{0.0f, 0.0f},
                        std::array<float, 2>{static_cast<float>(width), 0.0f},
                        std::array<float, 2>{static_cast<float>(width), static_cast<float>(height)},
                        std::array<float, 2>{0.0f, static_cast<float>(height)}
                    };

                    const auto normal_i = face_normal(face);
                    const std::array<float, 3> normal{static_cast<float>(normal_i[0]), static_cast<float>(normal_i[1]), static_cast<float>(normal_i[2])};

                    const auto base_index = static_cast<std::uint32_t>(result.vertices.size());
                    for (std::size_t i = 0; i < 4; ++i) {
                        result.vertices.push_back(vertex{corners[i], normal, uv[i], cell.id});
                    }

                    if (sign > 0) {
                        result.indices.insert(result.indices.end(), {base_index, base_index + 1, base_index + 2, base_index, base_index + 2, base_index + 3});
                    } else {
                        result.indices.insert(result.indices.end(), {base_index, base_index + 2, base_index + 1, base_index, base_index + 3, base_index + 2});
                    }

                    for (std::size_t dy = 0; dy < height; ++dy) {
                        for (std::size_t dx = 0; dx < width; ++dx) {
                            mask[u + dx + (v + dy) * du].filled = false;
                        }
                    }
                }
            }
        }
    }

    return result;
}

template <typename IsOpaque>
[[nodiscard]] mesh_result greedy_mesh_with_neighbor_chunks(const chunk_storage& chunk, const chunk_neighbors& neighbors,
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

    return greedy_mesh_with_neighbors(chunk, std::forward<IsOpaque>(is_opaque), neighbor_sampler);
}

inline mesh_result greedy_mesh_with_neighbor_chunks(const chunk_storage& chunk, const chunk_neighbors& neighbors) {
    return greedy_mesh_with_neighbor_chunks(chunk, neighbors, [](voxel_id id) { return id != voxel_id{}; });
}

template <typename IsOpaque>
[[nodiscard]] mesh_result greedy_mesh(const chunk_storage& chunk, IsOpaque&& is_opaque) {
    auto neighbor = [](const std::array<std::ptrdiff_t, 3>&) { return false; };
    return greedy_mesh_with_neighbors(chunk, std::forward<IsOpaque>(is_opaque), neighbor);
}

inline mesh_result greedy_mesh(const chunk_storage& chunk) {
    return greedy_mesh(chunk, [](voxel_id id) { return id != voxel_id{}; });
}

} // namespace almond::voxel::meshing
