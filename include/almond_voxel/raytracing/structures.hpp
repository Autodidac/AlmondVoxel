#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
#include <limits>

namespace almond::voxel::raytracing {

struct voxel_node_bounds {
    voxel_id min_material{std::numeric_limits<voxel_id>::max()};
    voxel_id max_material{0};
    bool occupied{false};

    void include(voxel_id id) {
        if (id == voxel_id{}) {
            return;
        }
        occupied = true;
        min_material = std::min(min_material, id);
        max_material = std::max(max_material, id);
    }
};

struct sparse_voxel_octree_node {
    voxel_node_bounds bounds{};
    std::array<std::uint32_t, 8> children{};
    std::uint32_t first_child{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t size{0};
    std::array<std::int32_t, 3> origin{};
    bool leaf{true};
};

class sparse_voxel_octree {
public:
    struct gpu_node {
        std::array<float, 3> origin{};
        float size{0.0f};
        std::array<std::uint32_t, 8> children{};
        std::uint32_t leaf{0};
        std::array<std::uint32_t, 2> material_range{};
    };

    sparse_voxel_octree() = default;

    void build(const chunk_storage& chunk, std::uint32_t max_depth = 5);

    [[nodiscard]] const sparse_voxel_octree_node& root() const { return nodes_.front(); }
    [[nodiscard]] const std::vector<sparse_voxel_octree_node>& nodes() const { return nodes_; }

    [[nodiscard]] std::vector<gpu_node> export_gpu_buffer() const;

private:
    void build_node(std::uint32_t node_index, const chunk_storage& chunk, std::uint32_t depth,
        std::array<std::uint32_t, 3> size, std::array<std::uint32_t, 3> offset, std::uint32_t max_depth);

    [[nodiscard]] static voxel_node_bounds accumulate_bounds(const chunk_storage& chunk,
        const std::array<std::uint32_t, 3>& size, const std::array<std::uint32_t, 3>& offset);

    std::vector<sparse_voxel_octree_node> nodes_{};
};

struct clipmap_level {
    std::array<std::uint32_t, 3> dimensions{};
    std::vector<voxel_node_bounds> cells{};
};

class clipmap_grid {
public:
    void build(const chunk_storage& chunk, std::uint32_t levels = 3);

    [[nodiscard]] const std::vector<clipmap_level>& levels() const noexcept { return levels_; }

private:
    std::vector<clipmap_level> levels_{};
};

class acceleration_cache {
public:
    struct region_entry {
        sparse_voxel_octree svo{};
        clipmap_grid clipmap{};
        bool dirty{true};
    };

    void update_region(const region_key& key, const chunk_storage& chunk);
    void invalidate_region(const region_key& key);

    [[nodiscard]] const region_entry* find(const region_key& key) const;
    [[nodiscard]] region_entry* assure(const region_key& key);

    void rebuild_dirty(const region_manager& manager);

private:
    std::unordered_map<region_key, region_entry, region_key_hash> regions_{};
};

inline void sparse_voxel_octree::build(const chunk_storage& chunk, std::uint32_t max_depth) {
    nodes_.clear();
    nodes_.push_back({});
    auto extent = chunk.extent();
    build_node(0, chunk, 0, {extent.x, extent.y, extent.z}, {0, 0, 0}, max_depth);
}

inline voxel_node_bounds sparse_voxel_octree::accumulate_bounds(const chunk_storage& chunk,
    const std::array<std::uint32_t, 3>& size, const std::array<std::uint32_t, 3>& offset) {
    voxel_node_bounds bounds;
    const auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < size[2]; ++z) {
        for (std::uint32_t y = 0; y < size[1]; ++y) {
            for (std::uint32_t x = 0; x < size[0]; ++x) {
                const auto px = offset[0] + x;
                const auto py = offset[1] + y;
                const auto pz = offset[2] + z;
                if (!voxels.contains(px, py, pz)) {
                    continue;
                }
                bounds.include(voxels(px, py, pz));
            }
        }
    }
    if (!bounds.occupied) {
        bounds.min_material = 0;
    }
    return bounds;
}

inline void sparse_voxel_octree::build_node(std::uint32_t node_index, const chunk_storage& chunk, std::uint32_t depth,
    std::array<std::uint32_t, 3> size, std::array<std::uint32_t, 3> offset, std::uint32_t max_depth) {
    auto& node = nodes_[node_index];
    node.bounds = accumulate_bounds(chunk, size, offset);
    node.origin = {static_cast<std::int32_t>(offset[0]), static_cast<std::int32_t>(offset[1]),
        static_cast<std::int32_t>(offset[2])};
    node.size = size[0];
    node.leaf = depth >= max_depth || size[0] <= 1 || size[1] <= 1 || size[2] <= 1 || !node.bounds.occupied;

    if (node.leaf) {
        node.first_child = std::numeric_limits<std::uint32_t>::max();
        node.children.fill(std::numeric_limits<std::uint32_t>::max());
        return;
    }

    node.leaf = false;
    node.first_child = static_cast<std::uint32_t>(nodes_.size());
    const std::array<std::uint32_t, 3> child_size{
        std::max<std::uint32_t>(1, size[0] / 2), std::max<std::uint32_t>(1, size[1] / 2), std::max<std::uint32_t>(1, size[2] / 2)};

    for (std::uint32_t child = 0; child < 8; ++child) {
        nodes_.push_back({});
        node.children[child] = node.first_child + child;
        std::array<std::uint32_t, 3> child_offset = offset;
        if (child & 1) {
            child_offset[0] += child_size[0];
        }
        if (child & 2) {
            child_offset[1] += child_size[1];
        }
        if (child & 4) {
            child_offset[2] += child_size[2];
        }
        build_node(node.children[child], chunk, depth + 1, child_size, child_offset, max_depth);
    }
}

inline std::vector<sparse_voxel_octree::gpu_node> sparse_voxel_octree::export_gpu_buffer() const {
    std::vector<gpu_node> buffer;
    buffer.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        gpu_node encoded;
        encoded.origin = {static_cast<float>(node.origin[0]), static_cast<float>(node.origin[1]),
            static_cast<float>(node.origin[2])};
        encoded.size = static_cast<float>(node.size);
        encoded.children = node.children;
        encoded.leaf = node.leaf ? 1U : 0U;
        encoded.material_range = {node.bounds.min_material, node.bounds.max_material};
        buffer.push_back(encoded);
    }
    return buffer;
}

inline void clipmap_grid::build(const chunk_storage& chunk, std::uint32_t levels) {
    levels_.clear();
    levels_.reserve(levels);
    auto extent = chunk.extent();
    std::array<std::uint32_t, 3> dims{extent.x, extent.y, extent.z};

    for (std::uint32_t level = 0; level < levels; ++level) {
        clipmap_level entry;
        entry.dimensions = dims;
        entry.cells.resize(static_cast<std::size_t>(dims[0]) * dims[1] * dims[2]);
        const auto voxels = chunk.voxels();
        for (std::uint32_t z = 0; z < dims[2]; ++z) {
            for (std::uint32_t y = 0; y < dims[1]; ++y) {
                for (std::uint32_t x = 0; x < dims[0]; ++x) {
                    auto& cell = entry.cells[x + dims[0] * (y + dims[1] * z)];
                    if (voxels.contains(x, y, z)) {
                        cell.include(voxels(x, y, z));
                    }
                }
            }
        }
        levels_.push_back(std::move(entry));
        dims[0] = std::max(1U, dims[0] / 2);
        dims[1] = std::max(1U, dims[1] / 2);
        dims[2] = std::max(1U, dims[2] / 2);
    }
}

inline void acceleration_cache::update_region(const region_key& key, const chunk_storage& chunk) {
    auto& entry = regions_[key];
    entry.svo.build(chunk);
    entry.clipmap.build(chunk);
    entry.dirty = false;
}

inline void acceleration_cache::invalidate_region(const region_key& key) {
    if (auto it = regions_.find(key); it != regions_.end()) {
        it->second.dirty = true;
    } else {
        regions_[key].dirty = true;
    }
}

inline const acceleration_cache::region_entry* acceleration_cache::find(const region_key& key) const {
    if (auto it = regions_.find(key); it != regions_.end()) {
        return &it->second;
    }
    return nullptr;
}

inline acceleration_cache::region_entry* acceleration_cache::assure(const region_key& key) {
    return &regions_[key];
}

inline void acceleration_cache::rebuild_dirty(const region_manager& manager) {
    auto snapshots = manager.snapshot_loaded(true);
    for (const auto& snapshot : snapshots) {
        if (!snapshot.chunk) {
            continue;
        }
        auto it = regions_.find(snapshot.key);
        if (it == regions_.end() || it->second.dirty) {
            update_region(snapshot.key, *snapshot.chunk);
        }
    }
}

} // namespace almond::voxel::raytracing

