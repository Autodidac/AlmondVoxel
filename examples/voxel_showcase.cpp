#include "almond_voxel/almond_voxel.hpp"

#include "test_framework.hpp"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace almond::voxel;
using namespace almond::voxel::editing;
using almond::voxel::terrain::classic_heightfield;

namespace {

enum class mesher_choice {
    greedy,
    marching
};

enum class terrain_mode {
    smooth,
    classic
};

enum class render_mode {
    raster,
    path_traced
};

struct float3 {
    float x{};
    float y{};
    float z{};
};

float3 operator+(float3 a, float3 b) {
    return float3{a.x + b.x, a.y + b.y, a.z + b.z};
}

float3 operator-(float3 a, float3 b) {
    return float3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float3 operator*(float3 a, float s) {
    return float3{a.x * s, a.y * s, a.z * s};
}

float3 operator*(float s, float3 a) {
    return a * s;
}

float3 operator/(float3 a, float s) {
    return float3{a.x / s, a.y / s, a.z / s};
}

float dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float3 cross(float3 a, float3 b) {
    return float3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length(float3 v) {
    return std::sqrt(dot(v, v));
}

float3 normalize(float3 v) {
    const float len = length(v);
    if (len <= 1e-6f) {
        return float3{};
    }
    return v / len;
}

float3 clamp(float3 v, float min_v, float max_v) {
    return float3{
        std::clamp(v.x, min_v, max_v),
        std::clamp(v.y, min_v, max_v),
        std::clamp(v.z, min_v, max_v)
    };
}

float3 hadamard(float3 a, float3 b) {
    return float3{a.x * b.x, a.y * b.y, a.z * b.z};
}

float3 lerp(float3 a, float3 b, float t) {
    return a + (b - a) * t;
}

struct camera {
    float3 position{};
    float yaw{0.0f};
    float pitch{0.0f};
    float fov{70.0f * 3.14159265f / 180.0f};
    float near_plane{0.1f};
    float far_plane{600.0f};
};

struct camera_vectors {
    float3 forward{};
    float3 right{};
    float3 up{};
};

float3 forward_from_angles(float yaw, float pitch) {
    const float cos_pitch = std::cos(pitch);
    return float3{
        std::sin(yaw) * cos_pitch,
        std::cos(yaw) * cos_pitch,
        std::sin(pitch)
    };
}

camera_vectors compute_camera_vectors(const camera& cam) {
    const float3 world_up{0.0f, 0.0f, 1.0f};
    float3 forward = normalize(forward_from_angles(cam.yaw, cam.pitch));
    float3 right = cross(forward, world_up);
    if (length(right) <= 1e-6f) {
        right = float3{1.0f, 0.0f, 0.0f};
    } else {
        right = normalize(right);
    }
    float3 up = cross(right, forward);
    return camera_vectors{forward, right, normalize(up)};
}

struct projection_result {
    SDL_FPoint point{};
    float depth{};
    bool visible{};
};

projection_result project_perspective(float3 position, const camera& cam, const camera_vectors& vectors, int width, int height)
{
    projection_result result{};
    const float3 relative = position - cam.position;
    const float x = dot(relative, vectors.right);
    const float y = dot(relative, vectors.up);
    const float z = dot(relative, vectors.forward);

    if (z <= cam.near_plane || z >= cam.far_plane) {
        result.visible = false;
        return result;
    }

    const float safe_width = static_cast<float>(std::max(width, 1));
    const float safe_height = static_cast<float>(std::max(height, 1));
    const float aspect = safe_width / safe_height;
    const float f = 1.0f / std::tan(cam.fov * 0.5f);

    const float ndc_x = (x * f / aspect) / z;
    const float ndc_y = (y * f) / z;

    result.point = SDL_FPoint{(ndc_x * 0.5f + 0.5f) * safe_width, (0.5f - ndc_y * 0.5f) * safe_height};
    result.depth = z;
    result.visible = true;
    return result;
}

struct clip_vertex {
    float x{};
    float y{};
    float z{};
    SDL_FColor color{};
};

clip_vertex make_clip_vertex(const float3& position, const SDL_FColor& color,
    const camera& cam, const camera_vectors& vectors)
{
    const float3 relative = position - cam.position;
    clip_vertex vertex{};
    vertex.x = dot(relative, vectors.right);
    vertex.y = dot(relative, vectors.up);
    vertex.z = dot(relative, vectors.forward);
    vertex.color = color;
    return vertex;
}

clip_vertex interpolate_vertex(const clip_vertex& a, const clip_vertex& b, float plane_z)
{
    const float delta_z = b.z - a.z;
    if (std::abs(delta_z) <= 1e-6f) {
        return a;
    }

    const float t = std::clamp((plane_z - a.z) / delta_z, 0.0f, 1.0f);
    clip_vertex result{};
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    result.color.r = a.color.r + (b.color.r - a.color.r) * t;
    result.color.g = a.color.g + (b.color.g - a.color.g) * t;
    result.color.b = a.color.b + (b.color.b - a.color.b) * t;
    result.color.a = a.color.a + (b.color.a - a.color.a) * t;
    return result;
}

void clip_against_plane(const std::vector<clip_vertex>& input, std::vector<clip_vertex>& output,
    float plane_z, bool keep_greater)
{
    output.clear();
    if (input.empty()) {
        return;
    }

    const std::size_t count = input.size();
    for (std::size_t i = 0; i < count; ++i) {
        const clip_vertex& current = input[i];
        const clip_vertex& next = input[(i + 1) % count];
        const bool current_inside = keep_greater ? current.z >= plane_z : current.z <= plane_z;
        const bool next_inside = keep_greater ? next.z >= plane_z : next.z <= plane_z;

        if (current_inside && next_inside) {
            output.push_back(next);
        } else if (current_inside && !next_inside) {
            output.push_back(interpolate_vertex(current, next, plane_z));
        } else if (!current_inside && next_inside) {
            output.push_back(interpolate_vertex(current, next, plane_z));
            output.push_back(next);
        }
    }
}

SDL_Vertex make_projected_vertex(const clip_vertex& vertex, const camera& cam, int width, int height)
{
    SDL_Vertex result{};

    const float safe_width = static_cast<float>(std::max(width, 1));
    const float safe_height = static_cast<float>(std::max(height, 1));
    const float aspect = safe_width / safe_height;
    const float f = 1.0f / std::tan(cam.fov * 0.5f);

    const float ndc_x = (vertex.x * f / aspect) / vertex.z;
    const float ndc_y = (vertex.y * f) / vertex.z;

    result.position = SDL_FPoint{
        (ndc_x * 0.5f + 0.5f) * safe_width,
        (0.5f - ndc_y * 0.5f) * safe_height
    };
    result.color = vertex.color;
    result.tex_coord = SDL_FPoint{0.0f, 0.0f};
    return result;
}

float3 sky_color(float3 dir) {
    dir = normalize(dir);
    const float t = std::clamp(dir.z * 0.5f + 0.5f, 0.0f, 1.0f);
    return lerp(float3{0.6f, 0.7f, 0.85f}, float3{0.05f, 0.05f, 0.1f}, t);
}

float3 voxel_color(voxel_id id) {
    if (id == voxel_id{2}) {
        return float3{1.0f, 0.6f, 0.2f};
    }
    return id != voxel_id{} ? float3{0.32f, 0.55f, 0.32f} : float3{0.0f, 0.0f, 0.0f};
}

double smooth_height(double x, double y) {
    const double large_scale = std::sin(x * 0.01) * 18.0 + std::cos(y * 0.01) * 16.0;
    const double medium_scale = std::sin(x * 0.05 + y * 0.04) * 9.0;
    const double ridge = std::sin(x * 0.15) * std::cos(y * 0.11) * 4.0;
    const double valley = std::cos(x * 0.02 + y * 0.018) * 6.0;
    const double base = 42.0 + large_scale + medium_scale + ridge + valley;
    return std::clamp(base, 0.0, 96.0);
}

chunk_storage generate_smooth_chunk(const region_key& key, const chunk_extent& extent) {
    chunk_storage chunk{extent};
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < extent.z; ++z) {
        const std::int64_t world_z = static_cast<std::int64_t>(key.z) * static_cast<std::int64_t>(extent.z)
            + static_cast<std::int64_t>(z);
        const double sample_z = static_cast<double>(world_z) + 0.5;
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            const double world_y = static_cast<double>(key.y) * static_cast<double>(extent.y) + static_cast<double>(y);
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                const double world_x = static_cast<double>(key.x) * static_cast<double>(extent.x) + static_cast<double>(x);
                const double height = smooth_height(world_x, world_y);
                voxels(x, y, z) = sample_z <= height ? voxel_id{1} : voxel_id{};
            }
        }
    }
    return chunk;
}

struct world_state {
    chunk_extent extent;
    terrain_mode terrain{terrain_mode::smooth};
    mesher_choice mesher{mesher_choice::greedy};
    render_mode renderer{render_mode::raster};
    classic_heightfield classic;
    region_manager regions;
    std::unordered_set<region_key, region_key_hash> dirty_regions{};

    explicit world_state(chunk_extent dimensions)
        : extent{dimensions}
        , classic{dimensions}
        , regions{dimensions} {
        configure_loader();
    }

    void configure_loader() {
        if (terrain == terrain_mode::smooth) {
            regions.set_loader([ext = extent](const region_key& key) { return generate_smooth_chunk(key, ext); });
        } else {
            regions.set_loader([this](const region_key& key) { return classic(key); });
        }
    }

    void set_terrain_mode(terrain_mode mode) {
        if (terrain == mode) {
            return;
        }
        terrain = mode;
        regions = region_manager{extent};
        dirty_regions.clear();
        configure_loader();
    }

    void mark_dirty(const region_key& key) {
        dirty_regions.insert(key);
    }
};

struct ray {
    float3 origin{};
    float3 direction{};
};

struct ray_hit {
    editing::world_position cell{};
    editing::world_position previous{};
    float3 position{};
    block_face face{block_face::pos_z};
    voxel_id id{};
    float distance{0.0f};
};

voxel_id sample_voxel(region_manager& regions, const chunk_extent& extent, const world_position& pos) {
    const auto coords = split_world_position(pos, extent);
    auto ptr = regions.find(coords.region);
    chunk_storage* chunk_ptr = nullptr;
    if (ptr) {
        chunk_ptr = ptr.get();
    } else {
        chunk_ptr = &regions.assure(coords.region);
    }
    const auto voxels = static_cast<const chunk_storage&>(*chunk_ptr).voxels();
    if (!voxels.contains(coords.local[0], coords.local[1], coords.local[2])) {
        return voxel_id{};
    }
    return voxels(coords.local[0], coords.local[1], coords.local[2]);
}

std::optional<ray_hit> trace_voxel_ray(region_manager& regions, const chunk_extent& extent, const ray& r, float max_distance) {
    float3 direction = normalize(r.direction);
    if (length(direction) <= 1e-6f) {
        return std::nullopt;
    }

    float3 origin = r.origin;
    editing::world_position cell{
        static_cast<std::int64_t>(std::floor(origin.x)),
        static_cast<std::int64_t>(std::floor(origin.y)),
        static_cast<std::int64_t>(std::floor(origin.z))
    };
    editing::world_position previous = cell;

    const auto advance_axis = [](float pos, float dir, int step) {
        if (dir > 0.0f) {
            return (std::floor(pos) + 1.0f - pos) / dir;
        }
        if (dir < 0.0f) {
            return (pos - std::floor(pos)) / -dir;
        }
        return std::numeric_limits<float>::infinity();
    };

    const auto delta_axis = [](float dir) {
        if (dir == 0.0f) {
            return std::numeric_limits<float>::infinity();
        }
        return std::abs(1.0f / dir);
    };

    float t = 0.0f;
    int step_x = direction.x > 0.0f ? 1 : -1;
    int step_y = direction.y > 0.0f ? 1 : -1;
    int step_z = direction.z > 0.0f ? 1 : -1;

    float t_max_x = advance_axis(origin.x, direction.x, step_x);
    float t_max_y = advance_axis(origin.y, direction.y, step_y);
    float t_max_z = advance_axis(origin.z, direction.z, step_z);

    float t_delta_x = delta_axis(direction.x);
    float t_delta_y = delta_axis(direction.y);
    float t_delta_z = delta_axis(direction.z);

    auto current_id = sample_voxel(regions, extent, cell);
    if (current_id != voxel_id{}) {
        return ray_hit{cell, cell, origin, block_face::neg_z, current_id, 0.0f};
    }

    block_face face = block_face::pos_z;

    while (t <= max_distance) {
        if (t_max_x < t_max_y) {
            if (t_max_x < t_max_z) {
                previous = cell;
                cell.x += step_x;
                t = t_max_x;
                t_max_x += t_delta_x;
                face = step_x > 0 ? block_face::neg_x : block_face::pos_x;
            } else {
                previous = cell;
                cell.z += step_z;
                t = t_max_z;
                t_max_z += t_delta_z;
                face = step_z > 0 ? block_face::neg_z : block_face::pos_z;
            }
        } else {
            if (t_max_y < t_max_z) {
                previous = cell;
                cell.y += step_y;
                t = t_max_y;
                t_max_y += t_delta_y;
                face = step_y > 0 ? block_face::neg_y : block_face::pos_y;
            } else {
                previous = cell;
                cell.z += step_z;
                t = t_max_z;
                t_max_z += t_delta_z;
                face = step_z > 0 ? block_face::neg_z : block_face::pos_z;
            }
        }

        if (t > max_distance) {
            break;
        }

        current_id = sample_voxel(regions, extent, cell);
        if (current_id != voxel_id{}) {
            const float3 hit_position = origin + direction * t;
            return ray_hit{cell, previous, hit_position, face, current_id, t};
        }
    }
    return std::nullopt;
}

meshing::mesh_result build_mesh_for_region(region_manager& regions, const chunk_extent& extent, const region_key& key,
    mesher_choice mode) {
    auto& chunk = regions.assure(key);
    if (mode == mesher_choice::marching) {
        return meshing::marching_cubes_from_chunk(chunk);
    }
    return meshing::greedy_mesh(chunk);
}

void ensure_region_meshes(world_state& world, const float3& camera_pos,
    std::unordered_map<region_key, meshing::mesh_result, region_key_hash>& meshes) {
    const int radius = 3;
    const float chunk_size = static_cast<float>(world.extent.x);
    const int center_x = static_cast<int>(std::floor(camera_pos.x / chunk_size));
    const int center_y = static_cast<int>(std::floor(camera_pos.y / chunk_size));

    std::unordered_set<region_key, region_key_hash> needed;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                region_key key{center_x + dx, center_y + dy, dz};
                needed.insert(key);
                if (!meshes.contains(key)) {
                    meshes.emplace(key, build_mesh_for_region(world.regions, world.extent, key, world.mesher));
                }
            }
        }
    }

    for (auto it = meshes.begin(); it != meshes.end();) {
        if (needed.contains(it->first)) {
            ++it;
        } else {
            it = meshes.erase(it);
        }
    }
}

void rebuild_dirty_meshes(world_state& world,
    std::unordered_map<region_key, meshing::mesh_result, region_key_hash>& meshes) {
    for (const auto& key : world.dirty_regions) {
        meshes[key] = build_mesh_for_region(world.regions, world.extent, key, world.mesher);
    }
    world.dirty_regions.clear();
}

float3 shade_triangle_color(voxel_id id, const std::array<float, 3>& normal, mesher_choice mode) {
    float3 normal_vec = normalize(float3{normal[0], normal[1], normal[2]});
    const float3 light = normalize(float3{0.6f, 0.9f, 0.5f});
    const float diffuse = std::max(dot(normal_vec, light), 0.0f);
    const float ambient = 0.35f;
    const float intensity = std::clamp(ambient + diffuse * 0.7f, 0.0f, 1.0f);
    float3 base = voxel_color(id);
    if (mode == mesher_choice::marching) {
        base = lerp(base, float3{0.8f, 0.8f, 0.8f}, 0.35f);
    }
    return clamp(base * intensity, 0.0f, 1.0f);
}

void render_meshes(SDL_Renderer* renderer, const camera& cam, const camera_vectors& vectors, int width, int height,
    const std::unordered_map<region_key, meshing::mesh_result, region_key_hash>& meshes, mesher_choice mode,
    const chunk_extent& extent) {
    struct projected_triangle {
        SDL_Vertex vertices[3]{};
        float depth{};
    };

    std::vector<projected_triangle> projected;
    std::vector<clip_vertex> clip_work;
    std::vector<clip_vertex> clip_temp;
    clip_work.reserve(6);
    clip_temp.reserve(6);

    for (const auto& [region, mesh] : meshes) {
        (void)region;
        const auto& vertices = mesh.vertices;
        const auto& indices = mesh.indices;
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            const auto i0 = indices[i];
            const auto i1 = indices[i + 1];
            const auto i2 = indices[i + 2];

            const auto& v0 = vertices[i0];
            const auto& v1 = vertices[i1];
            const auto& v2 = vertices[i2];

            const float3 p0{v0.position[0], v0.position[1], v0.position[2]};
            const float3 p1{v1.position[0], v1.position[1], v1.position[2]};
            const float3 p2{v2.position[0], v2.position[1], v2.position[2]};

            const float3 world_offset{
                static_cast<float>(region.x) * static_cast<float>(extent.x),
                static_cast<float>(region.y) * static_cast<float>(extent.y),
                static_cast<float>(region.z) * static_cast<float>(extent.z)
            };

            const float3 color = shade_triangle_color(v0.material, v0.normal, mode);
            const SDL_FColor vertex_color{color.x, color.y, color.z, 1.0f};

            clip_work.clear();
            clip_temp.clear();

            clip_work.push_back(make_clip_vertex(p0 + world_offset, vertex_color, cam, vectors));
            clip_work.push_back(make_clip_vertex(p1 + world_offset, vertex_color, cam, vectors));
            clip_work.push_back(make_clip_vertex(p2 + world_offset, vertex_color, cam, vectors));

            clip_against_plane(clip_work, clip_temp, cam.near_plane, true);
            if (clip_temp.empty()) {
                continue;
            }

            clip_against_plane(clip_temp, clip_work, cam.far_plane, false);
            if (clip_work.size() < 3) {
                continue;
            }

            for (std::size_t corner = 1; corner + 1 < clip_work.size(); ++corner) {
                projected_triangle tri{};
                const clip_vertex& vtx0 = clip_work[0];
                const clip_vertex& vtx1 = clip_work[corner];
                const clip_vertex& vtx2 = clip_work[corner + 1];

                tri.vertices[0] = make_projected_vertex(vtx0, cam, width, height);
                tri.vertices[1] = make_projected_vertex(vtx1, cam, width, height);
                tri.vertices[2] = make_projected_vertex(vtx2, cam, width, height);
                tri.depth = (vtx0.z + vtx1.z + vtx2.z) / 3.0f;
                projected.push_back(tri);
            }
        }
    }

    std::sort(projected.begin(), projected.end(), [](const auto& lhs, const auto& rhs) { return lhs.depth > rhs.depth; });

    for (const auto& tri : projected) {
        const std::array<int, 3> indices{0, 1, 2};
        SDL_RenderGeometry(renderer, nullptr, tri.vertices, 3, indices.data(), 3);
    }
}

float random_float(std::mt19937& rng, float min_value, float max_value) {
    std::uniform_real_distribution<float> dist(min_value, max_value);
    return dist(rng);
}

float3 random_hemisphere(std::mt19937& rng, float3 normal) {
    float3 dir{};
    do {
        dir = float3{random_float(rng, -1.0f, 1.0f), random_float(rng, -1.0f, 1.0f), random_float(rng, -1.0f, 1.0f)};
    } while (length(dir) <= 1e-6f);
    dir = normalize(dir);
    if (dot(dir, normal) < 0.0f) {
        dir = dir * -1.0f;
    }
    return dir;
}

class voxel_particle_system {
public:
    void update(float dt, const camera& cam, const camera_vectors& vectors, region_manager& regions,
        const chunk_extent& extent, terrain_mode mode);
    void render(SDL_Renderer* renderer, const camera& cam, const camera_vectors& vectors, int width, int height) const;
    void reset() {
        particles_.clear();
        emission_accumulator_ = 0.0f;
    }

private:
    struct particle {
        float3 position{};
        float3 velocity{};
        float lifetime{0.0f};
    };

    std::vector<particle> particles_{};
    std::mt19937 rng_{0xCAFEF00Du};
    float emission_accumulator_{0.0f};
};

void voxel_particle_system::update(float dt, const camera& cam, const camera_vectors& vectors, region_manager& regions,
    const chunk_extent& extent, terrain_mode) {
    emission_accumulator_ += dt * 18.0f;
    while (emission_accumulator_ >= 1.0f) {
        emission_accumulator_ -= 1.0f;
        particle p{};
        const float spread = 1.2f;
        p.position = cam.position + vectors.forward * 2.5f + float3{0.0f, 0.0f, -0.5f};
        p.velocity = vectors.up * random_float(rng_, 2.5f, 4.0f)
            + vectors.right * random_float(rng_, -spread, spread)
            + vectors.forward * random_float(rng_, -1.2f, 1.2f);
        p.lifetime = random_float(rng_, 2.0f, 4.0f);
        particles_.push_back(p);
    }

    for (auto& p : particles_) {
        if (p.lifetime <= 0.0f) {
            continue;
        }
        p.lifetime -= dt;
        p.velocity.z -= 9.8f * dt;
        const float3 new_position = p.position + p.velocity * dt;
        const world_position cell{
            static_cast<std::int64_t>(std::floor(new_position.x)),
            static_cast<std::int64_t>(std::floor(new_position.y)),
            static_cast<std::int64_t>(std::floor(new_position.z))
        };
        if (sample_voxel(regions, extent, cell) != voxel_id{}) {
            p.velocity = p.velocity * -0.35f;
        } else {
            p.position = new_position;
        }
    }

    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const particle& p) { return p.lifetime <= 0.0f; }),
        particles_.end());
}

void voxel_particle_system::render(SDL_Renderer* renderer, const camera& cam, const camera_vectors& vectors, int width,
    int height) const {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(renderer, 255, 180, 90, 220);
    for (const auto& p : particles_) {
        projection_result proj = project_perspective(p.position, cam, vectors, width, height);
        if (!proj.visible) {
            continue;
        }
        SDL_FRect rect{proj.point.x - 2.0f, proj.point.y - 2.0f, 4.0f, 4.0f};
        SDL_RenderFillRect(renderer, &rect);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

class voxel_path_tracer {
public:
    voxel_path_tracer() = default;
    ~voxel_path_tracer() {
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }
    }

    void reset() {
        samples_ = 0;
        std::fill(accumulation_.begin(), accumulation_.end(), float3{});
    }

    void render(SDL_Renderer* renderer, int output_width, int output_height, const camera& cam, const camera_vectors& vectors,
        region_manager& regions, const chunk_extent& extent, std::size_t samples_per_frame = 1);

private:
    SDL_Texture* texture_{nullptr};
    int width_{0};
    int height_{0};
    std::vector<float3> accumulation_{};
    std::vector<std::uint32_t> framebuffer_{};
    std::size_t samples_{0};
    std::mt19937 rng_{0xBADC0FFE};

    ray make_camera_ray(const camera& cam, const camera_vectors& vectors, float u, float v) const;
    float3 trace_path(const ray& r, region_manager& regions, const chunk_extent& extent, std::size_t depth);
    static std::uint32_t pack_color(float3 color);
};

ray voxel_path_tracer::make_camera_ray(const camera& cam, const camera_vectors& vectors, float u, float v) const {
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    const float px = (2.0f * u - 1.0f) * std::tan(cam.fov * 0.5f) * aspect;
    const float py = (1.0f - 2.0f * v) * std::tan(cam.fov * 0.5f);
    float3 direction = normalize(vectors.forward + vectors.right * px + vectors.up * py);
    return ray{cam.position, direction};
}

float3 voxel_path_tracer::trace_path(const ray& r, region_manager& regions, const chunk_extent& extent, std::size_t depth) {
    if (depth >= 3) {
        return sky_color(r.direction);
    }
    auto hit = trace_voxel_ray(regions, extent, r, 350.0f);
    if (!hit) {
        return sky_color(r.direction);
    }

    const float3 normal = float3{static_cast<float>(face_normal(hit->face)[0]), static_cast<float>(face_normal(hit->face)[1]),
        static_cast<float>(face_normal(hit->face)[2])};
    const float3 hit_normal = normalize(normal);
    const float3 hit_color = voxel_color(hit->id);

    const float3 emission = hit->id == voxel_id{2} ? float3{2.0f, 1.4f, 0.6f} : float3{0.0f, 0.0f, 0.0f};
    const ray bounce{hit->position + hit_normal * 0.01f, random_hemisphere(rng_, hit_normal)};
    const float cos_term = std::max(dot(bounce.direction, hit_normal), 0.0f);
    const float3 indirect = trace_path(bounce, regions, extent, depth + 1) * cos_term;
    const float3 direct = hadamard(hit_color, float3{0.4f, 0.55f, 0.35f}) * std::max(dot(hit_normal, normalize(float3{0.6f, 0.8f, 0.5f})), 0.0f);
    return emission + hadamard(hit_color, indirect * 0.6f + direct * 0.7f);
}

std::uint32_t voxel_path_tracer::pack_color(float3 color) {
    const auto to_byte = [](float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        const float gamma = std::pow(clamped, 1.0f / 2.2f);
        return static_cast<std::uint8_t>(std::round(gamma * 255.0f));
    };
    const std::uint8_t r = to_byte(color.x);
    const std::uint8_t g = to_byte(color.y);
    const std::uint8_t b = to_byte(color.z);
    return (0xFFu << 24) | (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8)
        | static_cast<std::uint32_t>(b);
}

void voxel_path_tracer::render(SDL_Renderer* renderer, int output_width, int output_height, const camera& cam,
    const camera_vectors& vectors, region_manager& regions, const chunk_extent& extent, std::size_t samples_per_frame) {
    const int target_width = std::max(160, output_width / 2);
    const int target_height = std::max(120, output_height / 2);
    if (target_width != width_ || target_height != height_) {
        width_ = target_width;
        height_ = target_height;
        accumulation_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), float3{});
        framebuffer_.assign(accumulation_.size(), 0u);
        samples_ = 0;
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }
        texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width_, height_);
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
    }

    if (!texture_) {
        return;
    }

    std::uniform_real_distribution<float> jitter(0.0f, 1.0f);
    for (std::size_t sample = 0; sample < samples_per_frame; ++sample) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const float u = (static_cast<float>(x) + jitter(rng_)) / static_cast<float>(width_);
                const float v = (static_cast<float>(y) + jitter(rng_)) / static_cast<float>(height_);
                const ray camera_ray = make_camera_ray(cam, vectors, u, v);
                const float3 color = trace_path(camera_ray, regions, extent, 0);
                const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                    + static_cast<std::size_t>(x);
                accumulation_[index] = accumulation_[index] + color;
            }
        }
        ++samples_;
    }

    for (std::size_t i = 0; i < accumulation_.size(); ++i) {
        const float inv_samples = 1.0f / static_cast<float>(samples_);
        const float3 averaged = accumulation_[i] * inv_samples;
        framebuffer_[i] = pack_color(averaged);
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) == 0 && pixels != nullptr) {
        const std::size_t bytes = framebuffer_.size() * sizeof(std::uint32_t);
        std::memcpy(pixels, framebuffer_.data(), bytes);
        SDL_UnlockTexture(texture_);
    }

    SDL_FRect dst{0.0f, 0.0f, static_cast<float>(output_width), static_cast<float>(output_height)};
    SDL_RenderTexture(renderer, texture_, nullptr, &dst);
}

struct interaction_state {
    bool mouse_captured{true};
    bool camera_changed{true};
};

} // namespace

int main(int argc, char** argv) {
    using namespace almond::voxel;

    mesher_choice mesher = mesher_choice::greedy;
    terrain_mode terrain = terrain_mode::smooth;
    render_mode renderer = render_mode::raster;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--marching") {
            mesher = mesher_choice::marching;
        } else if (arg == "--classic") {
            terrain = terrain_mode::classic;
        } else if (arg == "--path-trace") {
            renderer = render_mode::path_traced;
        }
    }

    try {
        almond::voxel::test::run_tests();
    } catch (const std::exception& ex) {
        std::cerr << "Embedded tests failed: " << ex.what() << "\n";
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialise SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Almond Voxel Showcase", 1920, 1080, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer_handle = SDL_CreateRenderer(window, nullptr);
    if (renderer_handle == nullptr) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const chunk_extent extent{32, 32, 96};
    world_state world{extent};
    world.mesher = mesher;
    world.renderer = renderer;
    world.set_terrain_mode(terrain);

    camera cam{};
    cam.position = float3{0.0f, -90.0f, 70.0f};
    cam.pitch = -0.35f;

    voxel_particle_system particles;
    voxel_path_tracer tracer;

    bool running = true;
    std::uint64_t last_ticks = SDL_GetTicks();
    interaction_state interaction{};
    SDL_SetWindowRelativeMouseMode(window, interaction.mouse_captured ? SDL_TRUE : SDL_FALSE);

    std::unordered_map<region_key, meshing::mesh_result, region_key_hash> mesh_cache;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_F1) {
                    interaction.mouse_captured = !interaction.mouse_captured;
                    SDL_SetWindowRelativeMouseMode(window, interaction.mouse_captured ? SDL_TRUE : SDL_FALSE);
                } else if (event.key.key == SDLK_M) {
                    world.mesher = world.mesher == mesher_choice::greedy ? mesher_choice::marching : mesher_choice::greedy;
                    mesh_cache.clear();
                    tracer.reset();
                    std::cout << "Mesher set to " << (world.mesher == mesher_choice::greedy ? "greedy" : "marching") << "\n";
                } else if (event.key.key == SDLK_T) {
                    world.renderer = world.renderer == render_mode::raster ? render_mode::path_traced : render_mode::raster;
                    tracer.reset();
                    std::cout << "Renderer set to "
                              << (world.renderer == render_mode::raster ? "raster" : "voxel path tracer") << "\n";
                } else if (event.key.key == SDLK_C) {
                    world.set_terrain_mode(world.terrain == terrain_mode::smooth ? terrain_mode::classic : terrain_mode::smooth);
                    mesh_cache.clear();
                    tracer.reset();
                    std::cout << "Terrain mode set to "
                              << (world.terrain == terrain_mode::smooth ? "smooth" : "classic") << "\n";
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION && interaction.mouse_captured) {
                const float sensitivity = 0.0025f;
                cam.yaw += static_cast<float>(event.motion.xrel) * sensitivity;
                cam.pitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                const float limit = 1.55334306f;
                cam.pitch = std::clamp(cam.pitch, -limit, limit);
                interaction.camera_changed = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && interaction.mouse_captured) {
                const camera_vectors vectors = compute_camera_vectors(cam);
                const ray r{cam.position, vectors.forward};
                auto hit = trace_voxel_ray(world.regions, world.extent, r, 150.0f);
                if (!hit) {
                    continue;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    clear_voxel(world.regions, hit->cell);
                    world.mark_dirty(split_world_position(hit->cell, world.extent).region);
                    tracer.reset();
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    const auto adjacent = hit->previous;
                    set_voxel(world.regions, adjacent, voxel_id{2});
                    world.mark_dirty(split_world_position(adjacent, world.extent).region);
                    tracer.reset();
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    set_voxel(world.regions, hit->cell, voxel_id{1});
                    world.mark_dirty(split_world_position(hit->cell, world.extent).region);
                    tracer.reset();
                }
            }
        }

        const std::uint64_t now = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(now - last_ticks) / 1000.0f;
        last_ticks = now;

        const camera_vectors vectors = compute_camera_vectors(cam);
        const std::uint8_t* keyboard = SDL_GetKeyboardState(nullptr);
        float3 movement{};
        float speed = keyboard[SDL_SCANCODE_LSHIFT] ? 45.0f : 20.0f;
        float3 forward{vectors.forward.x, vectors.forward.y, 0.0f};
        if (length(forward) > 1e-6f) {
            forward = normalize(forward);
        }
        float3 right{vectors.right.x, vectors.right.y, 0.0f};
        if (length(right) > 1e-6f) {
            right = normalize(right);
        }
        if (keyboard[SDL_SCANCODE_W]) movement = movement + forward;
        if (keyboard[SDL_SCANCODE_S]) movement = movement - forward;
        if (keyboard[SDL_SCANCODE_A]) movement = movement - right;
        if (keyboard[SDL_SCANCODE_D]) movement = movement + right;
        if (keyboard[SDL_SCANCODE_SPACE]) movement = movement + float3{0.0f, 0.0f, 1.0f};
        if (keyboard[SDL_SCANCODE_LCTRL]) movement = movement - float3{0.0f, 0.0f, 1.0f};
        if (length(movement) > 1e-6f) {
            cam.position = cam.position + normalize(movement) * (speed * delta_seconds);
            interaction.camera_changed = true;
        }

        ensure_region_meshes(world, cam.position, mesh_cache);
        rebuild_dirty_meshes(world, mesh_cache);

        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer_handle, &output_width, &output_height);

        SDL_SetRenderDrawColor(renderer_handle, 18, 18, 28, 255);
        SDL_RenderClear(renderer_handle);

        if (world.renderer == render_mode::raster) {
            render_meshes(renderer_handle, cam, vectors, output_width, output_height, mesh_cache, world.mesher, world.extent);
        } else {
            tracer.render(renderer_handle, output_width, output_height, cam, vectors, world.regions, world.extent, 1);
        }

        particles.update(delta_seconds, cam, vectors, world.regions, world.extent, world.terrain);
        particles.render(renderer_handle, cam, vectors, output_width, output_height);

        SDL_RenderPresent(renderer_handle);
        interaction.camera_changed = false;
    }

    SDL_DestroyRenderer(renderer_handle);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
