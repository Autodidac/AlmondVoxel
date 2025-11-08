#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/core.hpp"
#include "almond_voxel/world_fwd.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace almond::voxel {

namespace navigation {

using nav_node_index = std::size_t;

struct nav_neighbor_config {
    float horizontal_cost{1.0f};
    float vertical_cost{1.0f};
    std::uint32_t max_step_height{1};
};

struct nav_build_config {
    std::uint32_t clearance{2};
    nav_neighbor_config neighbor{};
    std::function<bool(voxel_id)> is_solid{
        [](voxel_id id) { return id != voxel_id{}; }
    };
    std::function<float(const chunk_storage&, std::uint32_t, std::uint32_t, std::uint32_t)> sample_cost{
        [](const chunk_storage&, std::uint32_t, std::uint32_t, std::uint32_t) { return 1.0f; }
    };
};

struct nav_cell {
    bool walkable{false};
    float traversal_cost{1.0f};
};

struct nav_grid {
    chunk_extent extent{};
    std::vector<nav_cell> cells{};

    [[nodiscard]] std::size_t size() const noexcept { return cells.size(); }

    [[nodiscard]] bool contains(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        return extent.contains(x, y, z);
    }

    [[nodiscard]] nav_node_index index(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        return static_cast<nav_node_index>(x)
            + static_cast<nav_node_index>(extent.x) * (static_cast<nav_node_index>(y)
                + static_cast<nav_node_index>(extent.y) * static_cast<nav_node_index>(z));
    }

    [[nodiscard]] std::array<std::uint32_t, 3> coordinates(nav_node_index node) const noexcept {
        const auto xy = static_cast<nav_node_index>(extent.x) * static_cast<nav_node_index>(extent.y);
        const auto z = static_cast<std::uint32_t>(node / xy);
        const auto rem = node % xy;
        const auto y = static_cast<std::uint32_t>(rem / extent.x);
        const auto x = static_cast<std::uint32_t>(rem % extent.x);
        return {x, y, z};
    }

    [[nodiscard]] bool walkable(nav_node_index node) const noexcept {
        return node < cells.size() && cells[node].walkable;
    }

    [[nodiscard]] bool walkable(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        if (!contains(x, y, z)) {
            return false;
        }
        return walkable(index(x, y, z));
    }

    [[nodiscard]] float cost(nav_node_index node) const noexcept {
        return node < cells.size() ? cells[node].traversal_cost : 1.0f;
    }
};

[[nodiscard]] nav_grid build_nav_grid(const chunk_storage& chunk, const nav_build_config& config = {});

struct nav_edge {
    nav_node_index node{std::numeric_limits<nav_node_index>::max()};
    float cost{std::numeric_limits<float>::infinity()};
};

using neighbor_list = std::vector<nav_edge>;

void for_each_neighbor(const nav_grid& grid, nav_node_index node, const nav_neighbor_config& config,
    const std::function<void(nav_edge)>& visitor);

[[nodiscard]] neighbor_list neighbors(const nav_grid& grid, nav_node_index node, const nav_neighbor_config& config = {});

struct nav_path {
    std::vector<nav_node_index> nodes{};
    float total_cost{std::numeric_limits<float>::infinity()};
};

[[nodiscard]] std::optional<nav_path> a_star(const nav_grid& grid, nav_node_index start, nav_node_index goal,
    const nav_neighbor_config& config = {});

struct flow_field {
    static constexpr nav_node_index invalid_node = std::numeric_limits<nav_node_index>::max();

    chunk_extent extent{};
    std::vector<nav_node_index> next{};
    std::vector<float> distance{};
};

[[nodiscard]] flow_field compute_flow_field(const nav_grid& grid, nav_node_index goal, const nav_neighbor_config& config = {});

[[nodiscard]] std::vector<nav_node_index> follow_flow(const flow_field& field, nav_node_index start,
    std::size_t max_steps = 1024);

struct nav_region_view {
    region_key key{};
    std::shared_ptr<const nav_grid> grid;
};

struct nav_bridge {
    region_key from_region{};
    nav_node_index from_node{flow_field::invalid_node};
    region_key to_region{};
    nav_node_index to_node{flow_field::invalid_node};
    float cost{std::numeric_limits<float>::infinity()};
};

struct stitched_nav_graph {
    std::vector<nav_region_view> regions{};
    std::vector<nav_bridge> bridges{};
};

void stitch_neighbor_regions(const nav_neighbor_config& neighbor, chunk_extent extent, stitched_nav_graph& stitched);

} // namespace navigation

} // namespace almond::voxel

// Implementation

namespace almond::voxel::navigation {

inline nav_grid build_nav_grid(const chunk_storage& chunk, const nav_build_config& config) {
    nav_grid grid;
    grid.extent = chunk.extent();
    grid.cells.resize(grid.extent.volume());

    const auto voxels = chunk.voxels();
    const std::uint32_t clearance = std::max<std::uint32_t>(1, config.clearance);

    for (std::uint32_t z = 0; z < grid.extent.z; ++z) {
        for (std::uint32_t y = 0; y < grid.extent.y; ++y) {
            for (std::uint32_t x = 0; x < grid.extent.x; ++x) {
                const auto idx = grid.index(x, y, z);
                bool open = true;
                for (std::uint32_t h = 0; h < clearance; ++h) {
                    const std::uint32_t sample_y = y + h;
                    if (sample_y >= grid.extent.y) {
                        break;
                    }
                    if (config.is_solid(voxels(x, sample_y, z))) {
                        open = false;
                        break;
                    }
                }
                if (!open) {
                    continue;
                }
                const bool supported = (y == 0) || config.is_solid(voxels(x, y - 1, z));
                if (!supported) {
                    continue;
                }
                grid.cells[idx].walkable = true;
                grid.cells[idx].traversal_cost = config.sample_cost(chunk, x, y, z);
            }
        }
    }

    return grid;
}

inline void for_each_neighbor(const nav_grid& grid, nav_node_index node, const nav_neighbor_config& config,
    const std::function<void(nav_edge)>& visitor) {
    if (!grid.walkable(node)) {
        return;
    }

    const auto [x, y, z] = grid.coordinates(node);
    const std::array<std::array<int, 3>, 6> offsets{ {{{1, 0, 0}}, {{-1, 0, 0}}, {{0, 1, 0}}, {{0, -1, 0}}, {{0, 0, 1}}, {{0, 0, -1}}} };

    for (const auto& offset : offsets) {
        const int nx = static_cast<int>(x) + offset[0];
        const int ny = static_cast<int>(y) + offset[1];
        const int nz = static_cast<int>(z) + offset[2];
        if (nx < 0 || ny < 0 || nz < 0) {
            continue;
        }
        const auto ux = static_cast<std::uint32_t>(nx);
        const auto uy = static_cast<std::uint32_t>(ny);
        const auto uz = static_cast<std::uint32_t>(nz);
        if (!grid.contains(ux, uy, uz)) {
            continue;
        }
        if (std::abs(static_cast<int>(uy) - static_cast<int>(y)) > static_cast<int>(config.max_step_height)) {
            continue;
        }
        const auto neighbor_idx = grid.index(ux, uy, uz);
        if (!grid.walkable(neighbor_idx)) {
            continue;
        }
        float movement_cost = config.horizontal_cost;
        if (offset[1] != 0) {
            movement_cost = config.vertical_cost * static_cast<float>(std::abs(offset[1]));
        }
        const float weight = 0.5f * (grid.cost(node) + grid.cost(neighbor_idx));
        visitor(nav_edge{neighbor_idx, movement_cost * weight});
    }
}

inline neighbor_list neighbors(const nav_grid& grid, nav_node_index node, const nav_neighbor_config& config) {
    neighbor_list result;
    for_each_neighbor(grid, node, config, [&result](nav_edge edge) {
        result.push_back(edge);
    });
    return result;
}

inline float heuristic_distance(const nav_grid& grid, nav_node_index node, nav_node_index goal,
    const nav_neighbor_config& config) {
    const auto [x1, y1, z1] = grid.coordinates(node);
    const auto [x2, y2, z2] = grid.coordinates(goal);
    const float dx = static_cast<float>(std::abs(static_cast<int>(x1) - static_cast<int>(x2)));
    const float dy = static_cast<float>(std::abs(static_cast<int>(y1) - static_cast<int>(y2)));
    const float dz = static_cast<float>(std::abs(static_cast<int>(z1) - static_cast<int>(z2)));
    return (dx + dz) * config.horizontal_cost + dy * config.vertical_cost;
}

inline std::optional<nav_path> a_star(const nav_grid& grid, nav_node_index start, nav_node_index goal,
    const nav_neighbor_config& config) {
    if (!grid.walkable(start) || !grid.walkable(goal)) {
        return std::nullopt;
    }

    struct frontier_node {
        nav_node_index node;
        float priority;
        float cost;
    };

    struct compare {
        bool operator()(const frontier_node& lhs, const frontier_node& rhs) const noexcept {
            return lhs.priority > rhs.priority;
        }
    };

    std::priority_queue<frontier_node, std::vector<frontier_node>, compare> frontier;
    std::vector<float> g_score(grid.size(), std::numeric_limits<float>::infinity());
    std::vector<nav_node_index> came_from(grid.size(), flow_field::invalid_node);

    g_score[start] = 0.0f;
    frontier.push(frontier_node{start, heuristic_distance(grid, start, goal, config), 0.0f});

    while (!frontier.empty()) {
        const auto current = frontier.top();
        frontier.pop();

        if (current.node == goal) {
            nav_path path;
            path.total_cost = current.cost;
            nav_node_index node_it = goal;
            while (node_it != flow_field::invalid_node) {
                path.nodes.push_back(node_it);
                if (node_it == start) {
                    break;
                }
                node_it = came_from[node_it];
            }
            std::reverse(path.nodes.begin(), path.nodes.end());
            return path;
        }

        for_each_neighbor(grid, current.node, config, [&](nav_edge edge) {
            const float tentative = g_score[current.node] + edge.cost;
            if (tentative + 1e-6f < g_score[edge.node]) {
                g_score[edge.node] = tentative;
                came_from[edge.node] = current.node;
                const float priority = tentative + heuristic_distance(grid, edge.node, goal, config);
                frontier.push(frontier_node{edge.node, priority, tentative});
            }
        });
    }

    return std::nullopt;
}

inline flow_field compute_flow_field(const nav_grid& grid, nav_node_index goal, const nav_neighbor_config& config) {
    flow_field field;
    field.extent = grid.extent;
    field.next.assign(grid.size(), flow_field::invalid_node);
    field.distance.assign(grid.size(), std::numeric_limits<float>::infinity());

    if (!grid.walkable(goal)) {
        return field;
    }

    struct queue_node {
        nav_node_index node;
        float cost;
    };

    struct compare {
        bool operator()(const queue_node& lhs, const queue_node& rhs) const noexcept {
            return lhs.cost > rhs.cost;
        }
    };

    std::priority_queue<queue_node, std::vector<queue_node>, compare> queue;
    queue.push(queue_node{goal, 0.0f});
    field.distance[goal] = 0.0f;
    field.next[goal] = goal;

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();
        if (current.cost > field.distance[current.node] + 1e-6f) {
            continue;
        }

        for_each_neighbor(grid, current.node, config, [&](nav_edge edge) {
            const float candidate = current.cost + edge.cost;
            if (candidate + 1e-6f < field.distance[edge.node]) {
                field.distance[edge.node] = candidate;
                field.next[edge.node] = current.node;
                queue.push(queue_node{edge.node, candidate});
            }
        });
    }

    return field;
}

inline std::vector<nav_node_index> follow_flow(const flow_field& field, nav_node_index start, std::size_t max_steps) {
    std::vector<nav_node_index> path;
    if (start >= field.next.size()) {
        return path;
    }

    nav_node_index current = start;
    for (std::size_t i = 0; i < max_steps; ++i) {
        path.push_back(current);
        const nav_node_index next = field.next[current];
        if (next == flow_field::invalid_node) {
            path.clear();
            return path;
        }
        if (next == current) {
            break;
        }
        current = next;
    }
    return path;
}

inline void stitch_pair(const nav_neighbor_config& neighbor, chunk_extent extent, const nav_region_view& from,
    const nav_region_view& to, stitched_nav_graph& stitched) {
    const int dx = to.key.x - from.key.x;
    const int dy = to.key.y - from.key.y;
    const int dz = to.key.z - from.key.z;
    const int manhattan = std::abs(dx) + std::abs(dy) + std::abs(dz);
    if (manhattan != 1 || !from.grid || !to.grid) {
        return;
    }

    const auto add_bridge = [&](std::uint32_t fx, std::uint32_t fy, std::uint32_t fz, std::uint32_t tx, std::uint32_t ty,
                                 std::uint32_t tz) {
        const auto from_index = from.grid->index(fx, fy, fz);
        const auto to_index = to.grid->index(tx, ty, tz);
        if (!from.grid->walkable(from_index) || !to.grid->walkable(to_index)) {
            return;
        }
        if (std::abs(static_cast<int>(ty) - static_cast<int>(fy)) > static_cast<int>(neighbor.max_step_height)) {
            return;
        }
        float movement_cost = neighbor.horizontal_cost;
        const int vertical_delta = static_cast<int>(ty) - static_cast<int>(fy);
        if (dy != 0) {
            movement_cost = neighbor.vertical_cost * static_cast<float>(std::abs(dy));
        }
        if (vertical_delta != 0) {
            movement_cost += neighbor.vertical_cost * static_cast<float>(std::abs(vertical_delta));
        }
        const float weight = 0.5f * (from.grid->cost(from_index) + to.grid->cost(to_index));
        stitched.bridges.push_back(nav_bridge{from.key, from_index, to.key, to_index, movement_cost * weight});
    };

    if (dx != 0) {
        const std::uint32_t fx = dx > 0 ? extent.x - 1 : 0;
        const std::uint32_t tx = dx > 0 ? 0 : extent.x - 1;
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t z = 0; z < extent.z; ++z) {
                for (int dy_off = -static_cast<int>(neighbor.max_step_height);
                     dy_off <= static_cast<int>(neighbor.max_step_height); ++dy_off) {
                    const int ty = static_cast<int>(y) + dy_off;
                    if (ty < 0 || ty >= static_cast<int>(extent.y)) {
                        continue;
                    }
                    add_bridge(fx, y, z, tx, static_cast<std::uint32_t>(ty), z);
                }
            }
        }
        return;
    }

    if (dz != 0) {
        const std::uint32_t fz = dz > 0 ? extent.z - 1 : 0;
        const std::uint32_t tz = dz > 0 ? 0 : extent.z - 1;
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                for (int dy_off = -static_cast<int>(neighbor.max_step_height);
                     dy_off <= static_cast<int>(neighbor.max_step_height); ++dy_off) {
                    const int ty = static_cast<int>(y) + dy_off;
                    if (ty < 0 || ty >= static_cast<int>(extent.y)) {
                        continue;
                    }
                    add_bridge(x, y, fz, x, static_cast<std::uint32_t>(ty), tz);
                }
            }
        }
        return;
    }

    if (dy != 0) {
        const std::uint32_t fy = dy > 0 ? extent.y - 1 : 0;
        const std::uint32_t ty = dy > 0 ? 0 : extent.y - 1;
        for (std::uint32_t x = 0; x < extent.x; ++x) {
            for (std::uint32_t z = 0; z < extent.z; ++z) {
                add_bridge(x, fy, z, x, ty, z);
            }
        }
    }
}

inline void stitch_neighbor_regions(const nav_neighbor_config& neighbor, chunk_extent extent, stitched_nav_graph& stitched) {
    for (std::size_t i = 0; i < stitched.regions.size(); ++i) {
        for (std::size_t j = i + 1; j < stitched.regions.size(); ++j) {
            stitch_pair(neighbor, extent, stitched.regions[i], stitched.regions[j], stitched);
            stitch_pair(neighbor, extent, stitched.regions[j], stitched.regions[i], stitched);
        }
    }
}

} // namespace almond::voxel::navigation

