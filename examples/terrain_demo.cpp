#include "almond_voxel/world.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
//#include <SDL3/SDL_main.h>
//#include <SDL3/SDL_opengl.h>
//#include <GL/glu.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>
#include <utility>

namespace {
using namespace almond::voxel;

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
    return float3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
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
    float far_plane{512.0f};
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

projection_result project_perspective(float3 position, const camera& cam, const camera_vectors& vectors, int width, int height) {
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

constexpr std::array<float3, 8> voxel_corners{std::to_array<float3>({
    float3{0.0f, 0.0f, 0.0f},
    float3{1.0f, 0.0f, 0.0f},
    float3{0.0f, 1.0f, 0.0f},
    float3{1.0f, 1.0f, 0.0f},
    float3{0.0f, 0.0f, 1.0f},
    float3{1.0f, 0.0f, 1.0f},
    float3{0.0f, 1.0f, 1.0f},
    float3{1.0f, 1.0f, 1.0f},
})};

constexpr std::array<std::pair<int, int>, 12> voxel_edges{std::to_array<std::pair<int, int>>({
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

void generate_heightmap(chunk_storage& chunk) {
    const auto extent = chunk.extent();
    auto voxels = chunk.voxels();
    const float scale_x = 2.0f / static_cast<float>(extent.x);
    const float scale_y = 2.0f / static_cast<float>(extent.y);

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                voxels(x, y, z) = voxel_id{};
            }
        }
    }

    for (std::uint32_t y = 0; y < extent.y; ++y) {
        for (std::uint32_t x = 0; x < extent.x; ++x) {
            const float fx = static_cast<float>(x) * scale_x - 1.0f;
            const float fy = static_cast<float>(y) * scale_y - 1.0f;
            const float height = (std::sin(fx * 3.14159f) + std::cos(fy * 3.14159f)) * 0.25f + 0.5f;
            const std::uint32_t max_z = static_cast<std::uint32_t>(std::clamp(height * static_cast<float>(extent.z), 0.0f,
                static_cast<float>(extent.z)));
            for (std::uint32_t z = 0; z < max_z && z < extent.z; ++z) {
                voxels(x, y, z) = voxel_id{1};
            }
        }
    }
}

void print_statistics(const meshing::mesh_result& mesh) {
    std::uint64_t checksum = 0;
    for (const auto& vertex : mesh.vertices) {
        checksum += static_cast<std::uint64_t>(vertex.position[0] * 17.0f + vertex.position[1] * 31.0f + vertex.position[2] * 47.0f)
            + vertex.id;
    }
    std::cout << "Terrain demo results\n";
    std::cout << "  Vertices : " << mesh.vertices.size() << "\n";
    std::cout << "  Indices  : " << mesh.indices.size() << "\n";
    std::cout << "  Checksum : 0x" << std::hex << checksum << std::dec << "\n";
    std::cout << "Press ESC or close the window to exit." << std::endl;
}
} // namespace

int main() {
    using namespace almond::voxel;

    region_manager world{cubic_extent(32)};
    const region_key key{0, 0, 0};
    auto& chunk = world.assure(key);

    chunk.fill(voxel_id{});
    generate_heightmap(chunk);

    const auto mesh = meshing::greedy_mesh(chunk);
    print_statistics(mesh);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialise SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Almond Voxel Terrain", 1280, 720, SDL_WINDOW_RESIZABLE);
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

    //if (!SDL_RendererHasFeature(renderer, SDL_RENDERER_FEATURE_RENDER_GEOMETRY)) {
    //    std::cerr << "SDL renderer does not support geometry rendering" << std::endl;
    //    SDL_DestroyRenderer(renderer);
    //    SDL_DestroyWindow(window);
    //    SDL_Quit();
    //    return 1;
    //}

    const auto extent = chunk.extent();
    const float3 center{static_cast<float>(extent.x) * 0.5f, static_cast<float>(extent.y) * 0.5f, static_cast<float>(extent.z) * 0.5f};

    camera cam{};
    cam.position = float3{center.x, -static_cast<float>(extent.y) * 0.75f, center.z + 10.0f};
    cam.yaw = 0.0f;
    cam.pitch = -0.35f;

    bool running = true;
    bool debug_view = false;
    bool mouse_captured = true;
    std::uint64_t previous_ticks = SDL_GetTicks();

    std::vector<projected_triangle> triangles;
    triangles.reserve(mesh.indices.size() / 3);
    std::vector<SDL_Vertex> draw_vertices;
    draw_vertices.reserve(mesh.indices.size());

    auto voxels = chunk.voxels();

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
        float move_speed = keyboard[SDL_SCANCODE_LSHIFT] ? 25.0f : 10.0f;
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

        SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
        SDL_RenderClear(renderer);

        triangles.clear();
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            projected_triangle tri{};
            bool visible = true;
            float depth_sum = 0.0f;
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto& src_vertex = mesh.vertices[mesh.indices[i + corner]];
                const float3 position = to_float3(src_vertex.position);
                const projection_result projected = project_perspective(position, cam, vectors, output_width, output_height);
                if (!projected.visible) {
                    visible = false;
                    break;
                }

                SDL_Vertex v{};
                v.position = projected.point;
                v.color = shade_color(src_vertex.id, src_vertex.normal);
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

        std::sort(triangles.begin(), triangles.end(), [](const projected_triangle& a, const projected_triangle& b) {
            return a.depth > b.depth;
        });

        draw_vertices.clear();
        draw_vertices.reserve(triangles.size() * 3);
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

            for (std::uint32_t z = 0; z < extent.z; ++z) {
                for (std::uint32_t y = 0; y < extent.y; ++y) {
                    for (std::uint32_t x = 0; x < extent.x; ++x) {
                        if (voxels(x, y, z) == voxel_id{}) {
                            continue;
                        }

                        const float3 base{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                        std::array<SDL_FPoint, voxel_corners.size()> projected_points{};
                        std::array<bool, voxel_corners.size()> visibility{};
                        for (std::size_t corner = 0; corner < voxel_corners.size(); ++corner) {
                            const float3 corner_position = add(base, voxel_corners[corner]);
                            const projection_result projected = project_perspective(corner_position, cam, vectors, output_width, output_height);
                            visibility[corner] = projected.visible;
                            projected_points[corner] = projected.point;
                        }

                        for (const auto& edge : voxel_edges) {
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
