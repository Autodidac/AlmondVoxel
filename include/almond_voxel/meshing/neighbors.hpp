#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/core.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace almond::voxel::meshing {

struct chunk_neighbors {
    const chunk_storage* pos_x{nullptr};
    const chunk_storage* neg_x{nullptr};
    const chunk_storage* pos_y{nullptr};
    const chunk_storage* neg_y{nullptr};
    const chunk_storage* pos_z{nullptr};
    const chunk_storage* neg_z{nullptr};

    [[nodiscard]] const chunk_storage* get(block_face face) const noexcept {
        switch (face) {
        case block_face::pos_x:
            return pos_x;
        case block_face::neg_x:
            return neg_x;
        case block_face::pos_y:
            return pos_y;
        case block_face::neg_y:
            return neg_y;
        case block_face::pos_z:
            return pos_z;
        case block_face::neg_z:
        default:
            return neg_z;
        }
    }
};

namespace detail {

struct neighbor_view {
    span3d<const voxel_id> voxels{};
    chunk_extent extent{};
    bool available{false};
};

inline std::array<neighbor_view, block_face_count> load_neighbor_views(const chunk_neighbors& neighbors) {
    std::array<neighbor_view, block_face_count> result{};

    const auto assign = [&](block_face face, const chunk_storage* storage) {
        if (storage == nullptr) {
            return;
        }
        auto& entry = result[static_cast<std::size_t>(face)];
        entry.voxels = storage->voxels();
        entry.extent = storage->extent();
        entry.available = true;
    };

    assign(block_face::pos_x, neighbors.pos_x);
    assign(block_face::neg_x, neighbors.neg_x);
    assign(block_face::pos_y, neighbors.pos_y);
    assign(block_face::neg_y, neighbors.neg_y);
    assign(block_face::pos_z, neighbors.pos_z);
    assign(block_face::neg_z, neighbors.neg_z);

    return result;
}

inline bool remap_to_neighbor_coords(const chunk_extent& extent, std::array<std::ptrdiff_t, 3>& coord,
    const std::array<neighbor_view, block_face_count>& neighbors, const neighbor_view*& out_view) {
    const std::array<std::ptrdiff_t, 3> dims{
        static_cast<std::ptrdiff_t>(extent.x),
        static_cast<std::ptrdiff_t>(extent.y),
        static_cast<std::ptrdiff_t>(extent.z)
    };

    int out_of_bounds_axes = 0;
    block_face face = block_face::pos_x;

    if (coord[0] < 0) {
        face = block_face::neg_x;
        ++out_of_bounds_axes;
    } else if (coord[0] >= dims[0]) {
        face = block_face::pos_x;
        ++out_of_bounds_axes;
    }

    if (coord[1] < 0) {
        face = block_face::neg_y;
        ++out_of_bounds_axes;
    } else if (coord[1] >= dims[1]) {
        face = block_face::pos_y;
        ++out_of_bounds_axes;
    }

    if (coord[2] < 0) {
        face = block_face::neg_z;
        ++out_of_bounds_axes;
    } else if (coord[2] >= dims[2]) {
        face = block_face::pos_z;
        ++out_of_bounds_axes;
    }

    if (out_of_bounds_axes != 1) {
        return false;
    }

    const auto index = static_cast<std::size_t>(face);
    const auto& view = neighbors[index];
    if (!view.available) {
        return false;
    }

    switch (face) {
    case block_face::neg_x:
        coord[0] += static_cast<std::ptrdiff_t>(view.extent.x);
        break;
    case block_face::pos_x:
        coord[0] -= dims[0];
        break;
    case block_face::neg_y:
        coord[1] += static_cast<std::ptrdiff_t>(view.extent.y);
        break;
    case block_face::pos_y:
        coord[1] -= dims[1];
        break;
    case block_face::neg_z:
        coord[2] += static_cast<std::ptrdiff_t>(view.extent.z);
        break;
    case block_face::pos_z:
        coord[2] -= dims[2];
        break;
    }

    if (coord[0] < 0 || coord[0] >= static_cast<std::ptrdiff_t>(view.extent.x) || coord[1] < 0
        || coord[1] >= static_cast<std::ptrdiff_t>(view.extent.y) || coord[2] < 0
        || coord[2] >= static_cast<std::ptrdiff_t>(view.extent.z)) {
        return false;
    }

    out_view = &view;
    return true;
}

} // namespace detail

} // namespace almond::voxel::meshing
