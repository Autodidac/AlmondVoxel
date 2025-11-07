#include "almond_voxel/world.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "almond_voxel/meshing/marching_cubes.hpp"

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using namespace almond::voxel;

enum class mesher_choice {
    greedy,
    marching
};

struct float3 {
    float x{};
    float y{};
    float z{};
};

float3 to_float3(const std::array<float, 3>& values) {
    return float3{values[0], values[1], values[2]};
}

float3 add(float3 a, float3 b) {
    return float3{a.x + b.x, a.y + b.y, a.z + b.z};
}

float3 subtract(float3 a, float3 b) {
    return float3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float3 scale(float3 v, float s) {
    return float3{v.x * s, v.y * s, v.z * s};
}

float3 cross(float3 a, float3 b) {
    return float3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length_squared(float3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float3 normalize(float3 v) {
    const float length_sq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (length_sq <= 0.0f) {
        return float3{};
    }
    const float inv_length = 1.0f / std::sqrt(length_sq);
    return float3{v.x * inv_length, v.y * inv_length, v.z * inv_length};
}

float dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

SDL_FColor shade_color(voxel_id id, const std::array<float, 3>& normal_values) {
    const float3 normal = normalize(to_float3(normal_values));
    const float3 light = normalize(float3{0.6f, 0.9f, 0.5f});
    const float intensity = std::clamp(dot(normal, light) * 0.5f + 0.5f, 0.2f, 1.0f);

    SDL_FColor base{};
    if (id == voxel_id{}) {
        base = SDL_FColor{200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 1.0f};
    } else {
        base = SDL_FColor{90.0f / 255.0f, 170.0f / 255.0f, 90.0f / 255.0f, 1.0f};
    }

    const auto scale_component = [intensity](float component) {
        const float scaled = component * intensity;
        return std::clamp(scaled, 0.0f, 1.0f);
    };

    base.r = scale_component(base.r);
    base.g = scale_component(base.g);
    base.b = scale_component(base.b);
    return base;
}

struct camera {
    float3 position{};
    float yaw{};
    float pitch{};
    float fov{70.0f * 3.14159265f / 180.0f};
    float near_plane{0.1f};
    float far_plane{1500.0f};
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
    if (length_squared(right) <= 1e-6f) {
        right = float3{1.0f, 0.0f, 0.0f};
    } else {
        right = normalize(right);
    }
    float3 up = normalize(cross(right, forward));
    return camera_vectors{forward, right, up};
}

struct projection_result {
    SDL_FPoint point{};
    float depth{};
    bool visible{};
};

projection_result project_perspective(float3 position, const camera& cam, const camera_vectors& vectors, int width, int height)
{
    projection_result result{};
    const float3 relative = subtract(position, cam.position);
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

struct projected_triangle {
    SDL_Vertex vertices[3]{};
    float depth{};
};

struct chunk_instance_key {
    region_key region{};
    int lod{0};

    [[nodiscard]] friend bool operator==(const chunk_instance_key&, const chunk_instance_key&) noexcept = default;
};

struct chunk_instance_hash {
    [[nodiscard]] std::size_t operator()(const chunk_instance_key& key) const noexcept {
        std::size_t seed = region_key_hash{}(key.region);
        seed ^= static_cast<std::size_t>(key.lod) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct chunk_mesh_entry {
    meshing::mesh_result mesh{};
    std::array<std::int64_t, 3> origin{};
    int cell_size{1};
    mesher_choice mode{mesher_choice::greedy};
};

struct lod_definition {
    int level{0};
    float min_distance{0.0f};
    float max_distance{0.0f};
    int cell_size{1};
};

constexpr std::array<std::pair<int, int>, 12> box_edges{std::to_array<std::pair<int, int>>({
    std::pair{0, 1},
    std::pair{1, 3},
    std::pair{3, 2},
    std::pair{2, 0},
    std::pair{4, 5},
    std::pair{5, 7},
    std::pair{7, 6},
    std::pair{6, 4},
    std::pair{0, 4},
    std::pair{1, 5},
    std::pair{2, 6},
    std::pair{3, 7},
})};

double terrain_height(double x, double y) {
    const double large_scale = std::sin(x * 0.005) * 30.0 + std::cos(y * 0.005) * 28.0;
    const double medium_scale = std::sin(x * 0.04 + y * 0.03) * 12.0;
    const double ridge = std::sin(x * 0.12) * std::cos(y * 0.11) * 4.0;
    const double valley = std::cos(x * 0.017 + y * 0.02) * 6.0;
    const double height = 48.0 + large_scale + medium_scale + ridge + valley;
    return std::clamp(height, 4.0, 88.0);
}

double sample_coordinate(std::int64_t origin, std::ptrdiff_t index, int cell_size) {
    return static_cast<double>(origin) + (static_cast<double>(index) + 0.5) * static_cast<double>(cell_size);
}

bool cell_is_solid(const std::array<std::int64_t, 3>& origin, int cell_size, const std::array<std::ptrdiff_t, 3>& coord) {
    const double sample_x = sample_coordinate(origin[0], coord[0], cell_size);
    const double sample_y = sample_coordinate(origin[1], coord[1], cell_size);
    const double terrain = terrain_height(sample_x, sample_y);
    const double sample_z = static_cast<double>(origin[2]) + (static_cast<double>(coord[2]) + 0.5);
    return sample_z < terrain;
}

chunk_mesh_entry build_chunk_mesh(
    const chunk_extent& extent,
    const std::array<std::int64_t, 3>& origin,
    int cell_size,
    mesher_choice mode) {
    chunk_storage chunk{extent};
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                std::array<std::ptrdiff_t, 3> coord{
                    static_cast<std::ptrdiff_t>(x),
                    static_cast<std::ptrdiff_t>(y),
                    static_cast<std::ptrdiff_t>(z)
                };
                voxels(x, y, z) = cell_is_solid(origin, cell_size, coord) ? voxel_id{1} : voxel_id{};
            }
        }
    }

    chunk_mesh_entry entry{};
    entry.mode = mode;

    if (mode == mesher_choice::marching) {
        auto density_sampler = [&](std::size_t vx, std::size_t vy, std::size_t vz) {
            float total = 0.0f;
            int count = 0;
            for (int dx = -1; dx <= 0; ++dx) {
                for (int dy = -1; dy <= 0; ++dy) {
                    for (int dz = -1; dz <= 0; ++dz) {
                        std::array<std::ptrdiff_t, 3> coord{
                            static_cast<std::ptrdiff_t>(vx) + dx,
                            static_cast<std::ptrdiff_t>(vy) + dy,
                            static_cast<std::ptrdiff_t>(vz) + dz
                        };
                        if (cell_is_solid(origin, cell_size, coord)) {
                            total += 1.0f;
                        }
                        ++count;
                    }
                }
            }
            if (count == 0) {
                return 0.0f;
            }
            return total / static_cast<float>(count);
        };

        auto material_sampler = [voxels](std::size_t x, std::size_t y, std::size_t z) {
            return voxels(x, y, z);
        };

        meshing::marching_cubes_config config{};
        config.iso_value = 0.5f;
        entry.mesh = meshing::marching_cubes(extent, density_sampler, material_sampler, config);
    } else {
        auto is_opaque = [](voxel_id id) { return id != voxel_id{}; };
        auto neighbor_sampler = [&](const std::array<std::ptrdiff_t, 3>& coord) {
            return cell_is_solid(origin, cell_size, coord);
        };
        entry.mesh = meshing::greedy_mesh_with_neighbors(chunk, is_opaque, neighbor_sampler);
    }
    entry.origin = origin;
    entry.cell_size = cell_size;

    return entry;
}

void update_required_chunks(const camera& cam, const chunk_extent& extent, const std::array<lod_definition, 3>& lods,
    mesher_choice mode, std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash>& cache) {
    std::unordered_set<chunk_instance_key, chunk_instance_hash> needed;

    for (const auto& lod : lods) {
        const float chunk_size = static_cast<float>(extent.x * lod.cell_size);
        if (chunk_size <= 0.0f) {
            continue;
        }

        const int base_x = static_cast<int>(std::floor(cam.position.x / chunk_size));
        const int base_y = static_cast<int>(std::floor(cam.position.y / chunk_size));
        const int radius = static_cast<int>(std::ceil(lod.max_distance / chunk_size)) + 1;

        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                region_key region{base_x + dx, base_y + dy, 0};
                const std::array<std::int64_t, 3> origin{
                    static_cast<std::int64_t>(region.x) * static_cast<std::int64_t>(extent.x) * lod.cell_size,
                    static_cast<std::int64_t>(region.y) * static_cast<std::int64_t>(extent.y) * lod.cell_size,
                    static_cast<std::int64_t>(region.z) * static_cast<std::int64_t>(extent.z)
                };

                const float center_x = static_cast<float>(origin[0]) + chunk_size * 0.5f;
                const float center_y = static_cast<float>(origin[1]) + chunk_size * 0.5f;
                const float dx_world = center_x - cam.position.x;
                const float dy_world = center_y - cam.position.y;
                const float distance = std::sqrt(dx_world * dx_world + dy_world * dy_world);

                if (distance < lod.min_distance || distance > lod.max_distance) {
                    continue;
                }

                const chunk_instance_key key{region, lod.level};
                if (needed.insert(key).second) {
                    auto it = cache.find(key);
                    if (it == cache.end()) {
                        cache.emplace(key, build_chunk_mesh(extent, origin, lod.cell_size, mode));
                    } else if (it->second.mode != mode) {
                        it->second = build_chunk_mesh(extent, origin, lod.cell_size, mode);
                    }
                }
            }
        }
    }

    for (auto it = cache.begin(); it != cache.end();) {
        if (needed.contains(it->first)) {
            ++it;
        } else {
            it = cache.erase(it);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    using namespace almond::voxel;

    mesher_choice mesher_mode = mesher_choice::marching;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--marching-cubes" || arg == "--mesher=marching") {
            mesher_mode = mesher_choice::marching;
        } else if (arg == "--mesher=greedy") {
            mesher_mode = mesher_choice::greedy;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialise SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Almond Voxel Terrain", 2560, 1440, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const chunk_extent chunk_dimensions{32, 32, 96};
    const std::array<lod_definition, 3> lods{
        lod_definition{0, 0.0f, 200.0f, 1},
        lod_definition{1, 180.0f, 480.0f, 2},
        lod_definition{2, 440.0f, 1400.0f, 4},
    };

    camera cam{};
    cam.position = float3{0.0f, -180.0f, 90.0f};
    cam.yaw = 0.0f;
    cam.pitch = -0.45f;

    bool running = true;
    bool debug_view = false;
    bool mouse_captured = true;
    std::uint64_t previous_ticks = SDL_GetTicks();

    std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash> chunk_meshes;

    std::vector<projected_triangle> triangles;
    std::vector<SDL_Vertex> draw_vertices;

    SDL_SetWindowRelativeMouseMode(window, true);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_F3) {
                    debug_view = !debug_view;
                } else if (event.key.key == SDLK_F1) {
                    mouse_captured = !mouse_captured;
                    SDL_SetWindowRelativeMouseMode(window, mouse_captured ? true : false);
                } else if (event.key.key == SDLK_M) {
                    mesher_mode = mesher_mode == mesher_choice::greedy ? mesher_choice::marching : mesher_choice::greedy;
                    chunk_meshes.clear();
                    std::cout << "Switched mesher to "
                              << (mesher_mode == mesher_choice::greedy ? "greedy" : "marching cubes") << "\n";
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION && mouse_captured) {
                const float sensitivity = 0.0025f;
                cam.yaw += static_cast<float>(event.motion.xrel) * sensitivity;
                cam.pitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                const float pitch_limit = 1.55334306f;
                cam.pitch = std::clamp(cam.pitch, -pitch_limit, pitch_limit);
                if (cam.yaw > 3.14159265f) {
                    cam.yaw -= 6.2831853f;
                } else if (cam.yaw < -3.14159265f) {
                    cam.yaw += 6.2831853f;
                }
            }
        }

        const std::uint64_t current_ticks = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(current_ticks - previous_ticks) / 1000.0f;
        previous_ticks = current_ticks;

        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);

        const camera_vectors vectors = compute_camera_vectors(cam);

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float move_speed = keyboard[SDL_SCANCODE_LSHIFT] ? 45.0f : 15.0f;
        float3 move_delta{};
        float3 planar_forward{vectors.forward.x, vectors.forward.y, 0.0f};
        if (length_squared(planar_forward) > 1e-6f) {
            planar_forward = normalize(planar_forward);
        }
        float3 planar_right{vectors.right.x, vectors.right.y, 0.0f};
        if (length_squared(planar_right) > 1e-6f) {
            planar_right = normalize(planar_right);
        } else {
            planar_right = float3{1.0f, 0.0f, 0.0f};
        }
        if (keyboard[SDL_SCANCODE_W]) {
            move_delta = add(move_delta, planar_forward);
        }
        if (keyboard[SDL_SCANCODE_S]) {
            move_delta = subtract(move_delta, planar_forward);
        }
        if (keyboard[SDL_SCANCODE_A]) {
            move_delta = subtract(move_delta, planar_right);
        }
        if (keyboard[SDL_SCANCODE_D]) {
            move_delta = add(move_delta, planar_right);
        }
        if (keyboard[SDL_SCANCODE_SPACE]) {
            move_delta = add(move_delta, float3{0.0f, 0.0f, 1.0f});
        }
        if (keyboard[SDL_SCANCODE_LCTRL]) {
            move_delta = subtract(move_delta, float3{0.0f, 0.0f, 1.0f});
        }

        if (length_squared(move_delta) > 1e-6f) {
            move_delta = normalize(move_delta);
            cam.position = add(cam.position, scale(move_delta, move_speed * delta_seconds));
        }

        update_required_chunks(cam, chunk_dimensions, lods, mesher_mode, chunk_meshes);

        SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
        SDL_RenderClear(renderer);

        std::size_t total_triangles = 0;
        for (const auto& [key, chunk] : chunk_meshes) {
            (void)key;
            total_triangles += chunk.mesh.indices.size() / 3;
        }

        triangles.clear();
        triangles.reserve(total_triangles);
        draw_vertices.clear();
        draw_vertices.reserve(total_triangles * 3);

        const auto world_position = [](const chunk_mesh_entry& chunk, const meshing::vertex& vertex) {
            return float3{
                static_cast<float>(chunk.origin[0]) + static_cast<float>(vertex.position[0]) * static_cast<float>(chunk.cell_size),
                static_cast<float>(chunk.origin[1]) + static_cast<float>(vertex.position[1]) * static_cast<float>(chunk.cell_size),
                static_cast<float>(chunk.origin[2]) + vertex.position[2]
            };
        };

        for (const auto& [key, chunk] : chunk_meshes) {
            (void)key;
            const auto& mesh = chunk.mesh;
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const std::uint32_t i0 = mesh.indices[i];
                const std::uint32_t i1 = mesh.indices[i + 1];
                const std::uint32_t i2 = mesh.indices[i + 2];

                const float3 p0 = world_position(chunk, mesh.vertices[i0]);
                const float3 p1 = world_position(chunk, mesh.vertices[i1]);
                const float3 p2 = world_position(chunk, mesh.vertices[i2]);
                float3 face_normal = cross(subtract(p1, p0), subtract(p2, p0));
                if (length_squared(face_normal) > 0.0f) {
                    face_normal = normalize(face_normal);
                    const float3 view_vector = subtract(cam.position, p0);
                    if (dot(face_normal, view_vector) <= 0.0f) {
                        continue;
                    }
                }

                projected_triangle tri{};
                bool visible = true;
                float depth_sum = 0.0f;
                const std::array<std::uint32_t, 3> indices{i0, i1, i2};
                const std::array<float3, 3> positions{p0, p1, p2};

                for (std::size_t corner = 0; corner < indices.size(); ++corner) {
                    const std::uint32_t idx = indices[corner];
                    const float3& position = positions[corner];
                    const projection_result projected = project_perspective(position, cam, vectors, output_width, output_height);
                    if (!projected.visible) {
                        visible = false;
                        break;
                    }

                    SDL_Vertex v{};
                    v.position = projected.point;
                    v.color = shade_color(mesh.vertices[idx].id, mesh.vertices[idx].normal);
                    v.tex_coord = SDL_FPoint{0.0f, 0.0f};
                    tri.vertices[corner] = v;
                    depth_sum += projected.depth;
                }

                if (!visible) {
                    continue;
                }

                tri.depth = depth_sum / 3.0f;
                triangles.push_back(tri);
            }
        }

        std::sort(triangles.begin(), triangles.end(), [](const projected_triangle& a, const projected_triangle& b) {
            return a.depth > b.depth;
        });

        for (const auto& tri : triangles) {
            for (const auto& vertex : tri.vertices) {
                draw_vertices.push_back(vertex);
            }
        }

        if (!draw_vertices.empty()) {
            if (!SDL_RenderGeometry(renderer, nullptr, draw_vertices.data(), static_cast<int>(draw_vertices.size()), nullptr, 0)) {
                std::cerr << "Failed to render geometry: " << SDL_GetError() << "\n";
                running = false;
            }
        }

        if (debug_view) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 240, 240, 255, 80);

            for (const auto& [key, chunk] : chunk_meshes) {
                (void)key;
                const float base_x = static_cast<float>(chunk.origin[0]);
                const float base_y = static_cast<float>(chunk.origin[1]);
                const float base_z = static_cast<float>(chunk.origin[2]);
                const float width = static_cast<float>(chunk_dimensions.x * chunk.cell_size);
                const float depth = static_cast<float>(chunk_dimensions.y * chunk.cell_size);
                const float height = static_cast<float>(chunk_dimensions.z);

                const std::array<float3, 8> corners{
                    float3{base_x, base_y, base_z},
                    float3{base_x + width, base_y, base_z},
                    float3{base_x, base_y + depth, base_z},
                    float3{base_x + width, base_y + depth, base_z},
                    float3{base_x, base_y, base_z + height},
                    float3{base_x + width, base_y, base_z + height},
                    float3{base_x, base_y + depth, base_z + height},
                    float3{base_x + width, base_y + depth, base_z + height},
                };

                std::array<SDL_FPoint, corners.size()> projected_points{};
                std::array<bool, corners.size()> visibility{};

                for (std::size_t i = 0; i < corners.size(); ++i) {
                    const projection_result projected = project_perspective(corners[i], cam, vectors, output_width, output_height);
                    visibility[i] = projected.visible;
                    projected_points[i] = projected.point;
                }

                for (const auto& edge : box_edges) {
                    if (!visibility[edge.first] || !visibility[edge.second]) {
                        continue;
                    }
                    SDL_RenderLine(renderer,
                        projected_points[edge.first].x,
                        projected_points[edge.first].y,
                        projected_points[edge.second].x,
                        projected_points[edge.second].y);
                }
            }

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
