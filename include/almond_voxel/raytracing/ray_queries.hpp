#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/raytracing/structures.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <algorithm>
#include <limits>

namespace almond::voxel::raytracing {

struct ray {
    std::array<float, 3> origin{};
    std::array<float, 3> direction{};
};

struct voxel_hit {
    bool hit{false};
    std::array<int, 3> position{};
    float distance{0.0f};
    voxel_id material{0};
};

inline std::array<int, 3> floor_to_int(const std::array<float, 3>& value) {
    return {static_cast<int>(std::floor(value[0])), static_cast<int>(std::floor(value[1])),
        static_cast<int>(std::floor(value[2]))};
}

inline voxel_hit trace_voxels(const chunk_storage& chunk, const ray& query, float max_distance) {
    voxel_hit result;
    const auto voxels = chunk.voxels();
    if (voxels.empty()) {
        return result;
    }

    std::array<float, 3> inv_dir{};
    for (int axis = 0; axis < 3; ++axis) {
        inv_dir[axis] = std::abs(query.direction[axis]) > 1e-6f ? 1.0f / query.direction[axis] : std::numeric_limits<float>::max();
    }

    std::array<float, 3> pos = query.origin;
    std::array<int, 3> voxel_pos = floor_to_int(pos);

    std::array<float, 3> t_max{};
    std::array<float, 3> t_delta{};
    for (int axis = 0; axis < 3; ++axis) {
        if (query.direction[axis] > 0.0f) {
            t_max[axis] = ((static_cast<float>(voxel_pos[axis] + 1) - pos[axis]) * inv_dir[axis]);
            t_delta[axis] = std::abs(inv_dir[axis]);
        } else if (query.direction[axis] < 0.0f) {
            t_max[axis] = ((static_cast<float>(voxel_pos[axis]) - pos[axis]) * inv_dir[axis]);
            t_delta[axis] = std::abs(inv_dir[axis]);
        } else {
            t_max[axis] = std::numeric_limits<float>::infinity();
            t_delta[axis] = std::numeric_limits<float>::infinity();
        }
    }

    std::array<int, 3> step{
        query.direction[0] > 0.0f ? 1 : (query.direction[0] < 0.0f ? -1 : 0),
        query.direction[1] > 0.0f ? 1 : (query.direction[1] < 0.0f ? -1 : 0),
        query.direction[2] > 0.0f ? 1 : (query.direction[2] < 0.0f ? -1 : 0)};

    auto in_bounds = [&](const std::array<int, 3>& coords) {
        return coords[0] >= 0 && coords[1] >= 0 && coords[2] >= 0 && coords[0] < static_cast<int>(voxels.extent().x)
            && coords[1] < static_cast<int>(voxels.extent().y) && coords[2] < static_cast<int>(voxels.extent().z);
    };

    float distance = 0.0f;
    while (distance <= max_distance) {
        if (in_bounds(voxel_pos)) {
            voxel_id id = voxels(static_cast<std::size_t>(voxel_pos[0]), static_cast<std::size_t>(voxel_pos[1]),
                static_cast<std::size_t>(voxel_pos[2]));
            if (id != voxel_id{}) {
                result.hit = true;
                result.position = voxel_pos;
                result.distance = distance;
                result.material = id;
                return result;
            }
        }

        int axis = 0;
        if (t_max[1] < t_max[axis]) {
            axis = 1;
        }
        if (t_max[2] < t_max[axis]) {
            axis = 2;
        }

        distance = t_max[axis];
        voxel_pos[axis] += step[axis];
        t_max[axis] += t_delta[axis];

        if (!in_bounds(voxel_pos) && distance > max_distance) {
            break;
        }
    }

    return result;
}

struct cone_trace_desc {
    std::array<float, 3> origin{};
    std::array<float, 3> direction{};
    float max_distance{16.0f};
    float aperture{0.5f};
    std::uint32_t steps{8};
};

inline float cone_trace_occlusion(const chunk_storage& chunk, const cone_trace_desc& desc) {
    const auto voxels = chunk.voxels();
    if (voxels.empty()) {
        return 0.0f;
    }

    std::array<float, 3> dir = desc.direction;
    float length = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (length <= 1e-6f) {
        return 0.0f;
    }
    dir[0] /= length;
    dir[1] /= length;
    dir[2] /= length;

    float occlusion = 0.0f;
    for (std::uint32_t step = 0; step < desc.steps; ++step) {
        float t = (static_cast<float>(step) + 0.5f) / static_cast<float>(desc.steps);
        float radius = desc.aperture * t;
        float distance = desc.max_distance * t;
        std::array<float, 3> sample{
            desc.origin[0] + dir[0] * distance,
            desc.origin[1] + dir[1] * distance,
            desc.origin[2] + dir[2] * distance};

        std::array<int, 3> center = floor_to_int(sample);
        int radius_voxels = static_cast<int>(std::ceil(radius));
        for (int dz = -radius_voxels; dz <= radius_voxels; ++dz) {
            for (int dy = -radius_voxels; dy <= radius_voxels; ++dy) {
                for (int dx = -radius_voxels; dx <= radius_voxels; ++dx) {
                    std::array<int, 3> probe{center[0] + dx, center[1] + dy, center[2] + dz};
                    if (probe[0] < 0 || probe[1] < 0 || probe[2] < 0 || probe[0] >= static_cast<int>(voxels.extent().x)
                        || probe[1] >= static_cast<int>(voxels.extent().y)
                        || probe[2] >= static_cast<int>(voxels.extent().z)) {
                        continue;
                    }
                    voxel_id id = voxels(static_cast<std::size_t>(probe[0]), static_cast<std::size_t>(probe[1]),
                        static_cast<std::size_t>(probe[2]));
                    if (id != voxel_id{}) {
                        occlusion += 1.0f / static_cast<float>(desc.steps);
                        dz = radius_voxels + 1;
                        dy = radius_voxels + 1;
                        break;
                    }
                }
            }
        }
    }

    return std::clamp(occlusion, 0.0f, 1.0f);
}

inline void export_gpu_nodes(const acceleration_cache& cache, const region_key& key,
    std::vector<sparse_voxel_octree::gpu_node>& out_buffer) {
    if (const auto* entry = cache.find(key); entry != nullptr) {
        auto nodes = entry->svo.export_gpu_buffer();
        out_buffer.insert(out_buffer.end(), nodes.begin(), nodes.end());
    }
}

} // namespace almond::voxel::raytracing

