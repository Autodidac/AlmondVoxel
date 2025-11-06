#include "almond_voxel/world.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

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

float3 rotate_y(float3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return float3{c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

float3 rotate_x(float3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return float3{v.x, c * v.y - s * v.z, s * v.y + c * v.z};
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

SDL_Color shade_color(voxel_id id, const std::array<float, 3>& normal_values) {
    const float3 normal = normalize(to_float3(normal_values));
    const float3 light = normalize(float3{0.6f, 0.9f, 0.5f});
    const float intensity = std::clamp(dot(normal, light) * 0.5f + 0.5f, 0.2f, 1.0f);

    SDL_Color base{};
    if (id == voxel_id{}) {
        base = SDL_Color{200, 200, 200, 255};
    } else {
        base = SDL_Color{90, 170, 90, 255};
    }

    const auto scale_component = [intensity](std::uint8_t component) {
        const float scaled = static_cast<float>(component) * intensity;
        return static_cast<std::uint8_t>(std::clamp(scaled, 0.0f, 255.0f));
    };

    base.r = scale_component(base.r);
    base.g = scale_component(base.g);
    base.b = scale_component(base.b);
    return base;
}

SDL_FPoint project_isometric(float3 v, int width, int height) {
    const float safe_width = static_cast<float>(std::max(width, 1));
    const float safe_height = static_cast<float>(std::max(height, 1));
    const float base_scale = std::min(safe_width, safe_height) / 4.0f;
    const float scale = std::max(base_scale, 1.0f);

    const float iso_x = (v.x - v.y) * 0.5f * scale;
    const float iso_y = (v.x + v.y) * 0.25f * scale - v.z * 0.8f * scale;

    return SDL_FPoint{iso_x + safe_width * 0.5f, iso_y + safe_height * 0.65f};
}

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

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to initialise SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Almond Voxel Terrain", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const auto extent = chunk.extent();
    const float3 center{static_cast<float>(extent.x) * 0.5f, static_cast<float>(extent.y) * 0.5f, static_cast<float>(extent.z) * 0.5f};

    bool running = true;
    float angle = 0.0f;
    std::uint64_t previous_ticks = SDL_GetTicks();

    std::vector<SDL_Vertex> draw_vertices;
    draw_vertices.reserve(mesh.indices.size());

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        const std::uint64_t current_ticks = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(current_ticks - previous_ticks) / 1000.0f;
        previous_ticks = current_ticks;
        angle += delta_seconds * 0.5f;

        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);

        SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
        SDL_RenderClear(renderer);

        draw_vertices.clear();
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto& src_vertex = mesh.vertices[mesh.indices[i + corner]];
                float3 position = to_float3(src_vertex.position);
                position.x -= center.x;
                position.y -= center.y;
                position.z -= center.z;

                position = rotate_y(position, angle);
                position = rotate_x(position, -0.6f);

                const SDL_FPoint projected = project_isometric(position, output_width, output_height);

                SDL_Vertex v{};
                v.position = projected;
                v.color = shade_color(src_vertex.id, src_vertex.normal);
                v.tex_coord = SDL_FPoint{0.0f, 0.0f};
                draw_vertices.push_back(v);
            }
        }

        if (!draw_vertices.empty()) {
            if (SDL_RenderGeometry(renderer, nullptr, draw_vertices.data(), static_cast<int>(draw_vertices.size()), nullptr, 0) != 0) {
                std::cerr << "Failed to render geometry: " << SDL_GetError() << "\n";
                running = false;
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
