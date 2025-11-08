#include "almond_voxel/world.hpp"
#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "almond_voxel/meshing/marching_cubes.hpp"
#include "almond_voxel/terrain/classic.hpp"

#include "../tests/test_framework.hpp"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <cctype>
#include <cstdio>
#include <initializer_list>
#include <deque>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
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

enum class terrain_mode {
    smooth,
    classic
};

enum class debug_display_mode {
    off,
    wireframe,
    solid_chunks,
    air_chunks
};

constexpr std::string_view debug_mode_name(debug_display_mode mode) {
    switch (mode) {
    case debug_display_mode::off:
        return "off";
    case debug_display_mode::wireframe:
        return "wireframe";
    case debug_display_mode::solid_chunks:
        return "non-air chunks";
    case debug_display_mode::air_chunks:
        return "air-only";
    }
    return "off";
}

constexpr debug_display_mode cycle_debug_mode(debug_display_mode mode,
    mesher_choice mesher, terrain_mode terrain) {
    (void)mesher;
    (void)terrain;

    switch (mode) {
    case debug_display_mode::off:
        return debug_display_mode::wireframe;
    case debug_display_mode::wireframe:
        return debug_display_mode::solid_chunks;
    case debug_display_mode::solid_chunks:
        return debug_display_mode::air_chunks;
    case debug_display_mode::air_chunks:
        return debug_display_mode::off;
    }
    return debug_display_mode::off;
}

constexpr std::int32_t bedrock_layer_count = 4;
constexpr std::int32_t sunlight_clearance_blocks = 3;
constexpr std::int32_t sunlight_scan_height = 64;

constexpr bool collision_uses_heightfield(terrain_mode terrain, mesher_choice mesher) {
    if (terrain == terrain_mode::classic) {
        return true;
    }
    return mesher == mesher_choice::marching;
}

struct float3 {
    float x{};
    float y{};
    float z{};
};

constexpr int bitmap_font_width = 4;
constexpr int bitmap_font_height = 6;
constexpr int bitmap_font_spacing = 1;

struct bitmap_font_glyph {
    std::array<std::uint8_t, bitmap_font_height> rows{};
};

constexpr bitmap_font_glyph make_glyph(std::initializer_list<const char*> rows) {
    bitmap_font_glyph glyph{};
    std::size_t index = 0;
    for (const char* row : rows) {
        std::uint8_t bits = 0;
        for (int column = 0; column < bitmap_font_width && row[column] != '\0'; ++column) {
            bits <<= 1;
            bits |= static_cast<std::uint8_t>(row[column] == '#');
        }
        bits <<= (bitmap_font_width - 4);
        if (index < glyph.rows.size()) {
            glyph.rows[index++] = bits;
        }
    }
    return glyph;
}

struct bitmap_font_entry {
    char character;
    bitmap_font_glyph glyph;
};

constexpr bitmap_font_entry bitmap_font_table[] = {
    {'0', make_glyph({" ## ", "#  #", "#  #", "#  #", "#  #", " ## "})},
    {'1', make_glyph({"  # ", " ## ", "  # ", "  # ", "  # ", " ###"})},
    {'2', make_glyph({" ## ", "#  #", "   #", "  # ", " #  ", "####"})},
    {'3', make_glyph({"####", "   #", " ###", "   #", "#  #", " ## "})},
    {'4', make_glyph({"#  #", "#  #", "#  #", "####", "   #", "   #"})},
    {'5', make_glyph({"####", "#   ", "### ", "   #", "#  #", " ## "})},
    {'6', make_glyph({" ## ", "#   ", "### ", "#  #", "#  #", " ## "})},
    {'7', make_glyph({"####", "   #", "  # ", " #  ", "#   ", "#   "})},
    {'8', make_glyph({" ## ", "#  #", " ## ", "#  #", "#  #", " ## "})},
    {'9', make_glyph({" ## ", "#  #", "#  #", " ###", "   #", " ## "})},
    {'F', make_glyph({"####", "#   ", "### ", "#   ", "#   ", "#   "})},
    {'P', make_glyph({"### ", "#  #", "### ", "#   ", "#   ", "#   "})},
    {'S', make_glyph({" ###", "#   ", " ## ", "   #", "   #", "### "})},
    {':', make_glyph({"    ", " ## ", " ## ", "    ", " ## ", " ## "})},
    {'.', make_glyph({"    ", "    ", "    ", "    ", " ## ", " ## "})}
};

const bitmap_font_glyph* find_bitmap_font_glyph(char character) {
    for (const auto& entry : bitmap_font_table) {
        if (entry.character == character) {
            return &entry.glyph;
        }
    }
    return nullptr;
}

int measure_bitmap_text(std::string_view text, int scale) {
    int width = 0;
    bool has_drawn = false;
    for (char character : text) {
        if (character == ' ') {
            width += (bitmap_font_width + bitmap_font_spacing) * scale;
            has_drawn = true;
            continue;
        }
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        if (find_bitmap_font_glyph(upper)) {
            width += (bitmap_font_width + bitmap_font_spacing) * scale;
            has_drawn = true;
        }
    }
    if (has_drawn) {
        width -= bitmap_font_spacing * scale;
    }
    return width;
}

void draw_bitmap_text(SDL_Renderer* renderer, std::string_view text, int x, int y, int scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int cursor_x = x;
    for (char character : text) {
        if (character == ' ') {
            cursor_x += (bitmap_font_width + bitmap_font_spacing) * scale;
            continue;
        }
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        const bitmap_font_glyph* glyph = find_bitmap_font_glyph(upper);
        if (!glyph) {
            cursor_x += (bitmap_font_width + bitmap_font_spacing) * scale;
            continue;
        }
        for (int row = 0; row < bitmap_font_height; ++row) {
            const std::uint8_t bits = glyph->rows[row];
            for (int column = 0; column < bitmap_font_width; ++column) {
                const std::uint8_t mask = static_cast<std::uint8_t>(1u << (bitmap_font_width - 1 - column));
                if ((bits & mask) == 0) {
                    continue;
                }
                SDL_FRect rect{};
                rect.x = static_cast<float>(cursor_x + column * scale);
                rect.y = static_cast<float>(y + row * scale);
                rect.w = static_cast<float>(scale);
                rect.h = static_cast<float>(scale);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        cursor_x += (bitmap_font_width + bitmap_font_spacing) * scale;
    }
}

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

[[nodiscard]] std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
    if (divisor == 0) {
        return 0;
    }
    std::int64_t quotient = value / divisor;
    std::int64_t remainder = value % divisor;
    if (remainder < 0) {
        --quotient;
    }
    return quotient;
}

SDL_FColor shade_color(voxel_id id, const std::array<float, 3>& normal_values,
    mesher_choice mode, terrain_mode terrain) {
    const float3 normal = normalize(to_float3(normal_values));
    const float3 light = normalize(float3{0.6f, 0.9f, 0.5f});
    const float diffuse = std::max(dot(normal, light), 0.0f);
    const float ambient = 0.35f;
    const float intensity = std::clamp(ambient + diffuse * 0.65f, 0.0f, 1.0f);

    SDL_FColor base{};
    if (mode == mesher_choice::marching && terrain != terrain_mode::classic) {
        base = SDL_FColor{0.65f, 0.80f, 1.0f, 1.0f}; // light sky blue
    } else {
        if (id == voxel_id{}) {
            base = SDL_FColor{0.55f, 0.70f, 0.95f, 1.0f}; // light blue
        } else if (id == voxel_id{4}) {
            base = SDL_FColor{0.15f, 0.25f, 0.45f, 1.0f}; // dark navy
        } else if (id == voxel_id{3}) {
            base = SDL_FColor{0.47f, 0.35f, 0.22f, 1.0f}; // unchanged brown (still dirt)
        } else if (id == voxel_id{2}) {
            base = SDL_FColor{0.38f, 0.66f, 0.36f, 1.0f}; // grass green
        } else {
            base = SDL_FColor{0.35f, 0.67f, 0.35f, 1.0f}; // soft green
        }
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

struct player_state {
    float3 position{};
    float3 velocity{};
    bool on_ground{false};
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
    region_key region{};
    int lod{};
    std::uint32_t sequence{};
    mesher_choice mesher{mesher_choice::greedy};
    float3 normal{};
    std::array<float3, 3> camera_vertices{};
};

struct clip_vertex {
    float x{};
    float y{};
    float z{};
    SDL_FColor color{};
};

clip_vertex make_clip_vertex(const float3& position, const SDL_FColor& color,
    const camera& cam, const camera_vectors& vectors)
{
    const float3 relative = subtract(position, cam.position);
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

    const float clamped_z = std::max(vertex.z, cam.near_plane);
    const float ndc_x = (vertex.x * f / aspect) / clamped_z;
    const float ndc_y = (vertex.y * f) / clamped_z;

    result.position = SDL_FPoint{
        (ndc_x * 0.5f + 0.5f) * safe_width,
        (0.5f - ndc_y * 0.5f) * safe_height
    };
    result.color = vertex.color;
    result.tex_coord = SDL_FPoint{0.0f, 0.0f};
    return result;
}

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
    terrain_mode terrain{terrain_mode::smooth};
};

struct lod_definition {
    int level{0};
    float min_distance{0.0f};
    float max_distance{0.0f};
    int cell_size{1};
};

struct overlay_line_segment {
    SDL_FPoint start{};
    SDL_FPoint end{};
};

struct overlay_draw_group {
    SDL_Color color{};
    std::vector<overlay_line_segment> segments;
};

struct chunk_overlay_info {
    float base_x{};
    float base_y{};
    float base_z{};
    float width{};
    float depth{};
    float height{};
    bool is_air{false};
};

struct overlay_input {
    debug_display_mode mode{debug_display_mode::off};
    terrain_mode terrain{terrain_mode::smooth};
    camera cam{};
    camera_vectors vectors{};
    int output_width{0};
    int output_height{0};
    std::vector<projected_triangle> triangles;
    std::vector<chunk_overlay_info> chunks;
    std::size_t generation{0};
};

struct overlay_output {
    debug_display_mode mode{debug_display_mode::off};
    terrain_mode terrain{terrain_mode::smooth};
    std::vector<overlay_draw_group> groups;
    std::size_t generation{0};
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

struct depth_buffer {
    int width{};
    int height{};
    int downsample{1};
    std::vector<float> values;

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 && downsample > 0 && !values.empty();
    }

    [[nodiscard]] float sample(float screen_x, float screen_y) const noexcept {
        if (!valid()) {
            return std::numeric_limits<float>::infinity();
        }
        const float scale = static_cast<float>(downsample);
        int ix = static_cast<int>(std::floor(screen_x / scale));
        int iy = static_cast<int>(std::floor(screen_y / scale));
        if (ix < 0 || iy < 0 || ix >= width || iy >= height) {
            return std::numeric_limits<float>::infinity();
        }
        return values[static_cast<std::size_t>(iy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(ix)];
    }
};

float edge_function(const SDL_FPoint& a, const SDL_FPoint& b, const SDL_FPoint& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

depth_buffer build_depth_buffer(const overlay_input& input) {
    depth_buffer buffer{};
    if (input.output_width <= 0 || input.output_height <= 0 || input.triangles.empty()) {
        return buffer;
    }

    constexpr int depth_downsample = 2;
    buffer.downsample = depth_downsample;
    buffer.width = std::max(1, (input.output_width + depth_downsample - 1) / depth_downsample);
    buffer.height = std::max(1, (input.output_height + depth_downsample - 1) / depth_downsample);
    buffer.values.assign(static_cast<std::size_t>(buffer.width) * static_cast<std::size_t>(buffer.height), input.cam.far_plane);

    const auto to_buffer_point = [depth_downsample](const SDL_FPoint& point) {
        return SDL_FPoint{point.x / static_cast<float>(depth_downsample), point.y / static_cast<float>(depth_downsample)};
    };

    for (const auto& tri : input.triangles) {
        const SDL_FPoint p0 = to_buffer_point(tri.vertices[0].position);
        const SDL_FPoint p1 = to_buffer_point(tri.vertices[1].position);
        const SDL_FPoint p2 = to_buffer_point(tri.vertices[2].position);

        float area = edge_function(p0, p1, p2);
        if (std::abs(area) <= 1e-6f) {
            continue;
        }

        const float min_xf = std::floor(std::min({p0.x, p1.x, p2.x}));
        const float max_xf = std::ceil(std::max({p0.x, p1.x, p2.x}));
        const float min_yf = std::floor(std::min({p0.y, p1.y, p2.y}));
        const float max_yf = std::ceil(std::max({p0.y, p1.y, p2.y}));

        const int min_x = std::max(0, static_cast<int>(min_xf));
        const int max_x = std::min(buffer.width - 1, static_cast<int>(max_xf));
        const int min_y = std::max(0, static_cast<int>(min_yf));
        const int max_y = std::min(buffer.height - 1, static_cast<int>(max_yf));

        if (min_x > max_x || min_y > max_y) {
            continue;
        }

        const bool ccw = area > 0.0f;
        if (!ccw) {
            area = -area;
        }

        const float inv_z0 = 1.0f / std::max(tri.camera_vertices[0].z, input.cam.near_plane);
        const float inv_z1 = 1.0f / std::max(tri.camera_vertices[1].z, input.cam.near_plane);
        const float inv_z2 = 1.0f / std::max(tri.camera_vertices[2].z, input.cam.near_plane);

        for (int y = min_y; y <= max_y; ++y) {
            const float sample_y = static_cast<float>(y) + 0.5f;
            for (int x = min_x; x <= max_x; ++x) {
                const float sample_x = static_cast<float>(x) + 0.5f;
                float w0 = edge_function(p1, p2, SDL_FPoint{sample_x, sample_y});
                float w1 = edge_function(p2, p0, SDL_FPoint{sample_x, sample_y});
                float w2 = edge_function(p0, p1, SDL_FPoint{sample_x, sample_y});

                if (!ccw) {
                    w0 = -w0;
                    w1 = -w1;
                    w2 = -w2;
                }

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }

                const float lambda0 = w0 / area;
                const float lambda1 = w1 / area;
                const float lambda2 = w2 / area;

                const float inv_z = lambda0 * inv_z0 + lambda1 * inv_z1 + lambda2 * inv_z2;
                if (inv_z <= 0.0f) {
                    continue;
                }

                float depth = 1.0f / inv_z;
                depth = std::clamp(depth, input.cam.near_plane, input.cam.far_plane);

                const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(buffer.width)
                    + static_cast<std::size_t>(x);
                if (depth < buffer.values[index]) {
                    buffer.values[index] = depth;
                }
            }
        }
    }

    return buffer;
}

SDL_FPoint project_camera_point_to_screen(const float3& camera_point, const camera& cam, int width, int height) {
    const float safe_width = static_cast<float>(std::max(width, 1));
    const float safe_height = static_cast<float>(std::max(height, 1));
    const float aspect = safe_width / safe_height;
    const float f = 1.0f / std::tan(cam.fov * 0.5f);
    const float ndc_x = (camera_point.x * f / aspect) / camera_point.z;
    const float ndc_y = (camera_point.y * f) / camera_point.z;
    return SDL_FPoint{
        (ndc_x * 0.5f + 0.5f) * safe_width,
        (0.5f - ndc_y * 0.5f) * safe_height
    };
}

bool segment_fully_occluded(const float3& camera_start, const float3& camera_end,
    const SDL_FPoint& screen_start, const SDL_FPoint& screen_end, const camera& cam,
    int width, int height, const depth_buffer* depth, float depth_bias) {
    if (depth == nullptr || !depth->valid()) {
        return false;
    }

    const float segment_length = std::hypot(screen_end.x - screen_start.x, screen_end.y - screen_start.y);
    constexpr float sample_spacing = 6.0f;
    const int sample_count = std::max(1, static_cast<int>(std::ceil(segment_length / sample_spacing)));

    for (int i = 0; i <= sample_count; ++i) {
        const float t = sample_count == 0 ? 0.0f : static_cast<float>(i) / static_cast<float>(sample_count);
        const float3 camera_point{
            camera_start.x + (camera_end.x - camera_start.x) * t,
            camera_start.y + (camera_end.y - camera_start.y) * t,
            camera_start.z + (camera_end.z - camera_start.z) * t
        };

        if (camera_point.z <= cam.near_plane + 1e-4f) {
            return false;
        }
        if (camera_point.z >= cam.far_plane) {
            continue;
        }

        const SDL_FPoint projected = project_camera_point_to_screen(camera_point, cam, width, height);
        if (projected.x < 0.0f || projected.x >= static_cast<float>(width)
            || projected.y < 0.0f || projected.y >= static_cast<float>(height)) {
            return false;
        }

        const float occluder_depth = depth->sample(projected.x, projected.y);
        if (!(occluder_depth + depth_bias < camera_point.z)) {
            return false;
        }
    }

    return true;
}

overlay_output build_overlay_output(overlay_input input) {
    overlay_output output{};
    output.mode = input.mode;
    output.terrain = input.terrain;
    output.generation = input.generation;

    if (input.mode == debug_display_mode::off) {
        return output;
    }

    depth_buffer depth = build_depth_buffer(input);
    const depth_buffer* depth_ptr = depth.valid() ? &depth : nullptr;
    constexpr float overlay_depth_bias = 0.05f;

    if (input.mode == debug_display_mode::wireframe) {
        const SDL_Color wire_color = input.terrain == terrain_mode::classic
            ? SDL_Color{255, 210, 150, 180}
            : SDL_Color{170, 230, 255, 170};

        overlay_draw_group group{};
        group.color = wire_color;

        struct wire_edge_key {
            std::array<std::int32_t, 2> a{};
            std::array<std::int32_t, 2> b{};

            [[nodiscard]] bool operator==(const wire_edge_key&) const noexcept = default;
        };

        struct wire_edge_hash {
            [[nodiscard]] std::size_t operator()(const wire_edge_key& key) const noexcept {
                std::size_t seed = std::hash<std::int32_t>{}(key.a[0]);
                seed ^= std::hash<std::int32_t>{}(key.a[1]) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::int32_t>{}(key.b[0]) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::int32_t>{}(key.b[1]) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct wire_edge_value {
            SDL_FPoint a{};
            SDL_FPoint b{};
            float3 camera_a{};
            float3 camera_b{};
            std::array<std::int32_t, 3> normal_key{};
            std::uint32_t count{0};
            bool coplanar_duplicate{false};
        };

        std::unordered_map<wire_edge_key, wire_edge_value, wire_edge_hash> greedy_edges;

        const auto quantize_point = [](const SDL_FPoint& point) {
            constexpr float scale = 1024.0f;
            return std::array<std::int32_t, 2>{
                static_cast<std::int32_t>(std::lround(point.x * scale)),
                static_cast<std::int32_t>(std::lround(point.y * scale))
            };
        };

        const auto quantize_normal = [](const float3& normal) {
            constexpr float scale = 1024.0f;
            return std::array<std::int32_t, 3>{
                static_cast<std::int32_t>(std::lround(normal.x * scale)),
                static_cast<std::int32_t>(std::lround(normal.y * scale)),
                static_cast<std::int32_t>(std::lround(normal.z * scale))
            };
        };

        const auto add_greedy_edge = [&](const SDL_FPoint& start, const SDL_FPoint& end,
            const float3& normal, const float3& camera_start, const float3& camera_end) {
            auto quant_a = quantize_point(start);
            auto quant_b = quantize_point(end);
            if (quant_a == quant_b) {
                return;
            }

            SDL_FPoint ordered_start = start;
            SDL_FPoint ordered_end = end;
            float3 ordered_camera_start = camera_start;
            float3 ordered_camera_end = camera_end;
            if (quant_a[0] > quant_b[0] || (quant_a[0] == quant_b[0] && quant_a[1] > quant_b[1])) {
                std::swap(quant_a, quant_b);
                std::swap(ordered_start, ordered_end);
                std::swap(ordered_camera_start, ordered_camera_end);
            }

            const wire_edge_key key{quant_a, quant_b};
            auto& entry = greedy_edges[key];
            const auto normal_key = quantize_normal(normal);
            if (entry.count == 0) {
                entry.a = ordered_start;
                entry.b = ordered_end;
                entry.camera_a = ordered_camera_start;
                entry.camera_b = ordered_camera_end;
                entry.normal_key = normal_key;
            } else if (entry.normal_key == normal_key) {
                entry.coplanar_duplicate = true;
            }
            ++entry.count;
        };

        for (const auto& tri : input.triangles) {
            const SDL_FPoint& a = tri.vertices[0].position;
            const SDL_FPoint& b = tri.vertices[1].position;
            const SDL_FPoint& c = tri.vertices[2].position;
            const float3& camera_a = tri.camera_vertices[0];
            const float3& camera_b = tri.camera_vertices[1];
            const float3& camera_c = tri.camera_vertices[2];
            if (tri.mesher == mesher_choice::greedy) {
                add_greedy_edge(a, b, tri.normal, camera_a, camera_b);
                add_greedy_edge(b, c, tri.normal, camera_b, camera_c);
                add_greedy_edge(c, a, tri.normal, camera_c, camera_a);
            } else {
                if (!segment_fully_occluded(camera_a, camera_b, a, b, input.cam,
                        input.output_width, input.output_height, depth_ptr, overlay_depth_bias)) {
                    group.segments.push_back(overlay_line_segment{a, b});
                }
                if (!segment_fully_occluded(camera_b, camera_c, b, c, input.cam,
                        input.output_width, input.output_height, depth_ptr, overlay_depth_bias)) {
                    group.segments.push_back(overlay_line_segment{b, c});
                }
                if (!segment_fully_occluded(camera_c, camera_a, c, a, input.cam,
                        input.output_width, input.output_height, depth_ptr, overlay_depth_bias)) {
                    group.segments.push_back(overlay_line_segment{c, a});
                }
            }
        }

        for (const auto& [key, edge] : greedy_edges) {
            (void)key;
            if (edge.coplanar_duplicate) {
                continue;
            }
            if (segment_fully_occluded(edge.camera_a, edge.camera_b, edge.a, edge.b, input.cam,
                    input.output_width, input.output_height, depth_ptr, overlay_depth_bias)) {
                continue;
            }
            group.segments.push_back(overlay_line_segment{edge.a, edge.b});
        }

        if (!group.segments.empty()) {
            output.groups.push_back(std::move(group));
        }
        return output;
    }

    const SDL_Color mode_color = [&]() {
        if (input.mode == debug_display_mode::solid_chunks) {
            return input.terrain == terrain_mode::classic
                ? SDL_Color{255, 220, 150, 110}
                : SDL_Color{210, 235, 255, 90};
        }
        return input.terrain == terrain_mode::classic
            ? SDL_Color{255, 185, 170, 125}
            : SDL_Color{180, 195, 255, 130};
    }();

    overlay_draw_group group{};
    group.color = mode_color;

    for (const auto& chunk : input.chunks) {
        const bool should_draw = (input.mode == debug_display_mode::solid_chunks) ? !chunk.is_air : chunk.is_air;
        if (!should_draw) {
            continue;
        }

        const std::array<float3, 8> corners{
            float3{chunk.base_x, chunk.base_y, chunk.base_z},
            float3{chunk.base_x + chunk.width, chunk.base_y, chunk.base_z},
            float3{chunk.base_x, chunk.base_y + chunk.depth, chunk.base_z},
            float3{chunk.base_x + chunk.width, chunk.base_y + chunk.depth, chunk.base_z},
            float3{chunk.base_x, chunk.base_y, chunk.base_z + chunk.height},
            float3{chunk.base_x + chunk.width, chunk.base_y, chunk.base_z + chunk.height},
            float3{chunk.base_x, chunk.base_y + chunk.depth, chunk.base_z + chunk.height},
            float3{chunk.base_x + chunk.width, chunk.base_y + chunk.depth, chunk.base_z + chunk.height},
        };

        std::array<SDL_FPoint, corners.size()> projected_points{};
        std::array<bool, corners.size()> visibility{};
        std::array<float3, corners.size()> camera_points{};

        for (std::size_t i = 0; i < corners.size(); ++i) {
            const clip_vertex corner_clip = make_clip_vertex(corners[i], SDL_FColor{}, input.cam, input.vectors);
            camera_points[i] = float3{corner_clip.x, corner_clip.y, corner_clip.z};
            const projection_result projected = project_perspective(corners[i], input.cam, input.vectors,
                input.output_width, input.output_height);
            visibility[i] = projected.visible;
            projected_points[i] = projected.point;
        }

        for (const auto& edge : box_edges) {
            if (!visibility[edge.first] || !visibility[edge.second]) {
                continue;
            }
            if (segment_fully_occluded(camera_points[edge.first], camera_points[edge.second],
                    projected_points[edge.first], projected_points[edge.second], input.cam,
                    input.output_width, input.output_height, depth_ptr, overlay_depth_bias)) {
                continue;
            }
            group.segments.push_back(overlay_line_segment{projected_points[edge.first], projected_points[edge.second]});
        }
    }

    if (!group.segments.empty()) {
        output.groups.push_back(std::move(group));
    }

    return output;
}

double base_height_field(double x, double y) {
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

struct terrain_sampler {
    terrain_mode mode{terrain_mode::smooth};
    terrain::classic_config classic_cfg{classic_profile()};

    static terrain::classic_config classic_profile() {
        terrain::classic_config cfg{};
        cfg.surface_voxel = voxel_id{2};
        cfg.subsurface_voxel = voxel_id{3};
        cfg.bedrock_voxel = voxel_id{4};
        cfg.bedrock_layers = static_cast<std::uint32_t>(bedrock_layer_count);
        return cfg;
    }

    terrain_sampler(terrain_mode mode_value, const chunk_extent&)
        : mode{mode_value}
        , classic_cfg{classic_profile()} { }

    [[nodiscard]] double base_height(double x, double y) const {
        return base_height_field(x, y);
    }

    [[nodiscard]] double height_at(double x, double y) const {
        return base_height(x, y);
    }

    [[nodiscard]] voxel_id classify(double sample_z, double height, std::int64_t world_z) const {
        if (world_z < static_cast<std::int64_t>(classic_cfg.bedrock_layers)) {
            return classic_cfg.bedrock_voxel;
        }
        if (mode == terrain_mode::classic) {
            const auto& cfg = classic_cfg;
            if (sample_z <= height) {
                const double depth = height - sample_z;
                if (depth > static_cast<double>(cfg.bedrock_layers)) {
                    return cfg.subsurface_voxel;
                }
                if (depth <= 0.5) {
                    return cfg.surface_voxel;
                }
                return cfg.subsurface_voxel;
            }
            return voxel_id{};
        }
        return sample_z < height ? voxel_id{1} : voxel_id{};
    }

    [[nodiscard]] voxel_id voxel_at(std::int64_t world_x, std::int64_t world_y, std::int64_t world_z) const {
        const double sample_x = static_cast<double>(world_x) + 0.5;
        const double sample_y = static_cast<double>(world_y) + 0.5;
        const double sample_z = static_cast<double>(world_z) + 0.5;
        const double height = height_at(sample_x, sample_y);
        return classify(sample_z, height, world_z);
    }
};

struct voxel_coord {
    std::int64_t x{};
    std::int64_t y{};
    std::int64_t z{};

    [[nodiscard]] friend bool operator==(const voxel_coord&, const voxel_coord&) noexcept = default;
};

struct voxel_coord_hash {
    [[nodiscard]] std::size_t operator()(const voxel_coord& coord) const noexcept {
        std::size_t seed = std::hash<std::int64_t>{}(coord.x);
        seed ^= std::hash<std::int64_t>{}(coord.y) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
        seed ^= std::hash<std::int64_t>{}(coord.z) + 0x9E3779B185EBCA87ull + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct voxel_edit_state {
    mutable std::shared_mutex mutex;
    std::unordered_map<voxel_coord, voxel_id, voxel_coord_hash> overrides;
};

[[nodiscard]] voxel_id sample_voxel_with_overrides(const terrain_sampler& sampler,
    const voxel_edit_state* edits, std::int64_t world_x, std::int64_t world_y, std::int64_t world_z) {
    if (edits != nullptr) {
        voxel_coord key{world_x, world_y, world_z};
        if (auto it = edits->overrides.find(key); it != edits->overrides.end()) {
            return it->second;
        }
    }
    return sampler.voxel_at(world_x, world_y, world_z);
}

[[nodiscard]] std::optional<voxel_id> find_override(const voxel_edit_state* edits,
    std::int64_t world_x, std::int64_t world_y, std::int64_t world_z) {
    if (!edits) {
        return std::nullopt;
    }
    voxel_coord key{world_x, world_y, world_z};
    if (auto it = edits->overrides.find(key); it != edits->overrides.end()) {
        return it->second;
    }
    return std::nullopt;
}

[[nodiscard]] bool is_bedrock_voxel(const terrain_sampler& sampler, const voxel_edit_state* edits,
    std::int64_t world_x, std::int64_t world_y, std::int64_t world_z) {
    const voxel_id bedrock_id = sampler.classic_cfg.bedrock_voxel;
    if (bedrock_id == voxel_id{}) {
        return false;
    }
    if (auto override_value = find_override(edits, world_x, world_y, world_z)) {
        if (*override_value == bedrock_id) {
            return true;
        }
    }
    return sampler.voxel_at(world_x, world_y, world_z) == bedrock_id;
}

[[nodiscard]] bool heightfield_cell_contains_solid(std::int64_t world_x, std::int64_t world_y,
    std::int64_t world_z, const terrain_sampler& sampler) {
    for (int dz = 0; dz <= 1; ++dz) {
        const double sample_z = static_cast<double>(world_z + dz);
        for (int dy = 0; dy <= 1; ++dy) {
            const double sample_y = static_cast<double>(world_y + dy);
            for (int dx = 0; dx <= 1; ++dx) {
                const double sample_x = static_cast<double>(world_x + dx);
                const double terrain_height = sampler.height_at(sample_x, sample_y);
                if (sample_z <= terrain_height) {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool voxel_is_solid(const terrain_sampler& sampler, const voxel_edit_state* edits,
    std::int64_t world_x, std::int64_t world_y, std::int64_t world_z, bool use_heightfield = false) {
    if (auto override_id = find_override(edits, world_x, world_y, world_z)) {
        return *override_id != voxel_id{};
    }
    if (use_heightfield) {
        return heightfield_cell_contains_solid(world_x, world_y, world_z, sampler);
    }
    return sampler.voxel_at(world_x, world_y, world_z) != voxel_id{};
}

[[nodiscard]] bool box_intersects_voxels(const float3& center, const float3& half_extents,
    const terrain_sampler& sampler, const voxel_edit_state* edits, bool use_heightfield) {
    const float min_x = center.x - half_extents.x;
    const float max_x = center.x + half_extents.x;
    const float min_y = center.y - half_extents.y;
    const float max_y = center.y + half_extents.y;
    const float min_z = center.z - half_extents.z;
    const float max_z = center.z + half_extents.z;

    const std::int64_t block_min_x = static_cast<std::int64_t>(std::floor(min_x));
    const std::int64_t block_max_x = static_cast<std::int64_t>(std::floor(max_x));
    const std::int64_t block_min_y = static_cast<std::int64_t>(std::floor(min_y));
    const std::int64_t block_max_y = static_cast<std::int64_t>(std::floor(max_y));
    const std::int64_t block_min_z = static_cast<std::int64_t>(std::floor(min_z));
    const std::int64_t block_max_z = static_cast<std::int64_t>(std::floor(max_z));

    for (std::int64_t z = block_min_z; z <= block_max_z; ++z) {
        for (std::int64_t y = block_min_y; y <= block_max_y; ++y) {
            for (std::int64_t x = block_min_x; x <= block_max_x; ++x) {
                if (voxel_is_solid(sampler, edits, x, y, z, use_heightfield)) {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] std::optional<float3> find_sunlit_recovery_position(const float3& desired_center,
    const float3& half_extents, const terrain_sampler& sampler, const voxel_edit_state* edits,
    mesher_choice mode) {
    const bool use_heightfield = collision_uses_heightfield(sampler.mode, mode);
    const float min_x = desired_center.x - half_extents.x;
    const float max_x = desired_center.x + half_extents.x;
    const float min_y = desired_center.y - half_extents.y;
    const float max_y = desired_center.y + half_extents.y;
    const std::int64_t block_min_x = static_cast<std::int64_t>(std::floor(min_x));
    const std::int64_t block_max_x = static_cast<std::int64_t>(std::floor(max_x));
    const std::int64_t block_min_y = static_cast<std::int64_t>(std::floor(min_y));
    const std::int64_t block_max_y = static_cast<std::int64_t>(std::floor(max_y));

    const double terrain_height = sampler.height_at(desired_center.x, desired_center.y);
    std::int64_t base_search_z = static_cast<std::int64_t>(std::floor(terrain_height)) + 1;
    base_search_z = std::max<std::int64_t>(base_search_z, static_cast<std::int64_t>(bedrock_layer_count));
    const std::int64_t search_end = base_search_z + sunlight_scan_height;

    for (std::int64_t base_z = base_search_z; base_z < search_end; ++base_z) {
        bool has_clearance = true;
        for (std::int64_t dz = 0; dz < static_cast<std::int64_t>(sunlight_clearance_blocks); ++dz) {
            const std::int64_t z = base_z + dz;
            for (std::int64_t y = block_min_y; y <= block_max_y; ++y) {
                for (std::int64_t x = block_min_x; x <= block_max_x; ++x) {
                    if (voxel_is_solid(sampler, edits, x, y, z, use_heightfield)) {
                        has_clearance = false;
                        break;
                    }
                }
                if (!has_clearance) {
                    break;
                }
            }
            if (!has_clearance) {
                break;
            }
        }

        if (!has_clearance) {
            continue;
        }

        bool receives_sunlight = true;
        for (std::int64_t z = base_z + sunlight_clearance_blocks; z < search_end; ++z) {
            for (std::int64_t y = block_min_y; y <= block_max_y; ++y) {
                for (std::int64_t x = block_min_x; x <= block_max_x; ++x) {
                    if (voxel_is_solid(sampler, edits, x, y, z, use_heightfield)) {
                        receives_sunlight = false;
                        break;
                    }
                }
                if (!receives_sunlight) {
                    break;
                }
            }
            if (!receives_sunlight) {
                break;
            }
        }

        if (!receives_sunlight) {
            continue;
        }

        float3 candidate{desired_center.x, desired_center.y,
            static_cast<float>(base_z) + half_extents.z};
        if (!box_intersects_voxels(candidate, half_extents, sampler, edits, use_heightfield)) {
            return candidate;
        }
    }

    return std::nullopt;
}

void move_player_axis(float3& position, float& velocity_component, float delta, int axis,
    const float3& half_extents, const terrain_sampler& sampler, const voxel_edit_state* edits,
    mesher_choice mode, bool& on_ground) {
    if (delta == 0.0f) {
        return;
    }

    float3 offset{};
    if (axis == 0) {
        offset.x = delta;
    } else if (axis == 1) {
        offset.y = delta;
    } else {
        offset.z = delta;
    }

    const bool use_heightfield = collision_uses_heightfield(sampler.mode, mode);
    const auto intersects = [&](const float3& test_position) {
        return box_intersects_voxels(test_position, half_extents, sampler, edits, use_heightfield);
    };

    float3 start = position;
    position = add(position, offset);
    if (!intersects(position)) {
        return;
    }

    float3 last_safe = start;
    float t_low = 0.0f;
    float t_high = 1.0f;
    for (int i = 0; i < 12; ++i) {
        const float t_mid = 0.5f * (t_low + t_high);
        const float3 candidate = add(start, scale(offset, t_mid));
        if (intersects(candidate)) {
            t_high = t_mid;
        } else {
            last_safe = candidate;
            t_low = t_mid;
        }
    }
    position = last_safe;

    constexpr float epsilon = 0.001f;
    float3 separation{};
    if (axis == 0) {
        separation.x = (delta > 0.0f) ? -epsilon : epsilon;
    } else if (axis == 1) {
        separation.y = (delta > 0.0f) ? -epsilon : epsilon;
    } else {
        separation.z = (delta > 0.0f) ? -epsilon : epsilon;
    }

    const float3 adjusted = add(position, separation);
    if (!intersects(adjusted)) {
        position = adjusted;
    }

    if (axis == 2 && delta < 0.0f) {
        on_ground = true;
    }
    velocity_component = 0.0f;
}

struct voxel_raycast_hit {
    bool valid{false};
    voxel_coord hit{};
    voxel_coord previous{};
    bool has_previous{false};
};

[[nodiscard]] bool block_intersects_player(const voxel_coord& block, const float3& center, const float3& half_extents) {
    const float block_min_x = static_cast<float>(block.x);
    const float block_max_x = static_cast<float>(block.x + 1);
    const float block_min_y = static_cast<float>(block.y);
    const float block_max_y = static_cast<float>(block.y + 1);
    const float block_min_z = static_cast<float>(block.z);
    const float block_max_z = static_cast<float>(block.z + 1);

    const float player_min_x = center.x - half_extents.x;
    const float player_max_x = center.x + half_extents.x;
    const float player_min_y = center.y - half_extents.y;
    const float player_max_y = center.y + half_extents.y;
    const float player_min_z = center.z - half_extents.z;
    const float player_max_z = center.z + half_extents.z;

    if (player_max_x <= block_min_x || player_min_x >= block_max_x) {
        return false;
    }
    if (player_max_y <= block_min_y || player_min_y >= block_max_y) {
        return false;
    }
    if (player_max_z <= block_min_z || player_min_z >= block_max_z) {
        return false;
    }
    return true;
}

[[nodiscard]] voxel_raycast_hit raycast_voxels(float3 origin, float3 direction, float max_distance,
    const terrain_sampler& sampler, const voxel_edit_state* edits, bool use_heightfield) {
    voxel_raycast_hit result{};
    if (length_squared(direction) <= 1e-6f) {
        return result;
    }

    direction = normalize(direction);
    constexpr float step = 0.25f;
    float traveled = 0.0f;
    voxel_coord previous_block{std::numeric_limits<std::int64_t>::min(), 0, 0};

    while (traveled <= max_distance) {
        float3 sample = add(origin, scale(direction, traveled));
        voxel_coord block{
            static_cast<std::int64_t>(std::floor(sample.x)),
            static_cast<std::int64_t>(std::floor(sample.y)),
            static_cast<std::int64_t>(std::floor(sample.z))
        };

        if (block == previous_block) {
            traveled += step;
            continue;
        }
        previous_block = block;

        if (voxel_is_solid(sampler, edits, block.x, block.y, block.z, use_heightfield)) {
            result.valid = true;
            result.hit = block;
            return result;
        }

        result.previous = block;
        result.has_previous = true;
        traveled += step;
    }

    return result;
}

constexpr float player_radius = 0.4f;
constexpr float player_height = 1.8f;
constexpr float player_half_height = player_height * 0.5f;
constexpr float player_eye_height = 1.6f;
constexpr float player_eye_offset = player_eye_height - player_half_height;
constexpr float gravity_acceleration = -38.0f;
constexpr float jump_velocity = 12.0f;

bool cell_is_solid(const std::array<std::int64_t, 3>& origin, int cell_size, const std::array<std::ptrdiff_t, 3>& coord,
    const terrain_sampler& sampler, const voxel_edit_state* edits) {
    if (cell_size == 1 && edits != nullptr) {
        const std::int64_t world_x = origin[0] + coord[0];
        const std::int64_t world_y = origin[1] + coord[1];
        const std::int64_t world_z = origin[2] + coord[2];
        return voxel_is_solid(sampler, edits, world_x, world_y, world_z);
    }

    const double sample_x = sample_coordinate(origin[0], coord[0], cell_size);
    const double sample_y = sample_coordinate(origin[1], coord[1], cell_size);
    const double terrain = sampler.height_at(sample_x, sample_y);
    const double sample_z = static_cast<double>(origin[2]) + (static_cast<double>(coord[2]) + 0.5);
    const std::int64_t world_z = static_cast<std::int64_t>(std::floor(sample_z - 0.5));
    return sampler.classify(sample_z, terrain, world_z) != voxel_id{};
}

struct terrain_height_cache {
    std::vector<double> cell{};
    std::vector<double> vertex{};
    std::size_t cell_stride{};
    std::size_t vertex_stride{};
};

terrain_height_cache build_height_cache(const chunk_extent& extent, const std::array<std::int64_t, 3>& origin,
    int cell_size, const terrain_sampler& sampler) {
    terrain_height_cache cache{};
    const std::size_t cell_width = static_cast<std::size_t>(extent.x);
    const std::size_t cell_height = static_cast<std::size_t>(extent.y);
    const std::size_t vertex_width = static_cast<std::size_t>(extent.x) + 1;
    const std::size_t vertex_height = static_cast<std::size_t>(extent.y) + 1;

    cache.cell.resize(cell_width * cell_height);
    cache.vertex.resize(vertex_width * vertex_height);
    cache.cell_stride = cell_width;
    cache.vertex_stride = vertex_width;

    std::vector<double> cell_x(cell_width);
    for (std::size_t x = 0; x < cell_width; ++x) {
        cell_x[x] = sample_coordinate(origin[0], static_cast<std::ptrdiff_t>(x), cell_size);
    }

    std::vector<double> cell_y(cell_height);
    for (std::size_t y = 0; y < cell_height; ++y) {
        cell_y[y] = sample_coordinate(origin[1], static_cast<std::ptrdiff_t>(y), cell_size);
    }

    std::vector<double> vertex_x(vertex_width);
    for (std::size_t x = 0; x < vertex_width; ++x) {
        vertex_x[x] = static_cast<double>(origin[0]) + static_cast<double>(x) * static_cast<double>(cell_size);
    }

    std::vector<double> vertex_y(vertex_height);
    for (std::size_t y = 0; y < vertex_height; ++y) {
        vertex_y[y] = static_cast<double>(origin[1]) + static_cast<double>(y) * static_cast<double>(cell_size);
    }

    for (std::size_t y = 0; y < cell_height; ++y) {
        for (std::size_t x = 0; x < cell_width; ++x) {
            cache.cell[y * cache.cell_stride + x] = sampler.height_at(cell_x[x], cell_y[y]);
        }
    }

    for (std::size_t y = 0; y < vertex_height; ++y) {
        for (std::size_t x = 0; x < vertex_width; ++x) {
            cache.vertex[y * cache.vertex_stride + x] = sampler.height_at(vertex_x[x], vertex_y[y]);
        }
    }

    return cache;
}

meshing::mesh_result build_classic_height_mesh(const chunk_extent& extent,
    const std::array<std::int64_t, 3>& origin, int cell_size,
    const terrain_sampler& sampler, const terrain_height_cache& cache) {
    const std::size_t vertex_width = static_cast<std::size_t>(extent.x) + 1;
    const std::size_t vertex_height = static_cast<std::size_t>(extent.y) + 1;

    meshing::mesh_result mesh{};
    mesh.vertices.resize(vertex_width * vertex_height);

    const auto vertex_index = [vertex_width](std::size_t x, std::size_t y) {
        return y * vertex_width + x;
    };

    for (std::size_t y = 0; y < vertex_height; ++y) {
        for (std::size_t x = 0; x < vertex_width; ++x) {
            const std::size_t index = vertex_index(x, y);
            meshing::vertex vert{};
            vert.position = {
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(cache.vertex[index] - static_cast<double>(origin[2]))
            };

            const auto sample_height = [&](std::size_t sx, std::size_t sy) {
                const std::size_t clamped_x = std::clamp<std::size_t>(sx, 0, vertex_width - 1);
                const std::size_t clamped_y = std::clamp<std::size_t>(sy, 0, vertex_height - 1);
                return cache.vertex[vertex_index(clamped_x, clamped_y)];
            };

            const double left = sample_height(x > 0 ? x - 1 : x, y);
            const double right = sample_height(x + 1, y);
            const double down = sample_height(x, y > 0 ? y - 1 : y);
            const double up = sample_height(x, y + 1);
            const double dx = (right - left) / (2.0 * static_cast<double>(cell_size));
            const double dy = (up - down) / (2.0 * static_cast<double>(cell_size));

            float3 normal_vector = float3{
                static_cast<float>(-dx),
                static_cast<float>(-dy),
                1.0f
            };
            normal_vector = normalize(normal_vector);
            if (length_squared(normal_vector) <= 1e-6f) {
                normal_vector = float3{0.0f, 0.0f, 1.0f};
            }

            vert.normal = {normal_vector.x, normal_vector.y, normal_vector.z};
            vert.uv = {static_cast<float>(x), static_cast<float>(y)};
            vert.id = sampler.classic_cfg.surface_voxel;
            mesh.vertices[index] = vert;
        }
    }

    for (std::size_t y = 0; y + 1 < vertex_height; ++y) {
        for (std::size_t x = 0; x + 1 < vertex_width; ++x) {
            const std::uint32_t v0 = static_cast<std::uint32_t>(vertex_index(x, y));
            const std::uint32_t v1 = static_cast<std::uint32_t>(vertex_index(x + 1, y));
            const std::uint32_t v2 = static_cast<std::uint32_t>(vertex_index(x, y + 1));
            const std::uint32_t v3 = static_cast<std::uint32_t>(vertex_index(x + 1, y + 1));

            mesh.indices.push_back(v0);
            mesh.indices.push_back(v1);
            mesh.indices.push_back(v2);
            mesh.indices.push_back(v1);
            mesh.indices.push_back(v3);
            mesh.indices.push_back(v2);
        }
    }

    return mesh;
}

chunk_mesh_entry build_chunk_mesh(
    const chunk_extent& extent,
    const std::array<std::int64_t, 3>& origin,
    int cell_size,
    mesher_choice mode,
    const terrain_sampler& sampler,
    const voxel_edit_state* edits) {
    const auto height_cache = build_height_cache(extent, origin, cell_size, sampler);
    const std::size_t cell_stride = height_cache.cell_stride;
    const std::size_t vertex_stride = height_cache.vertex_stride;

    chunk_mesh_entry entry{};
    entry.mode = mode;
    entry.terrain = sampler.mode;

    if (sampler.mode == terrain_mode::classic) {
        entry.mesh = build_classic_height_mesh(extent, origin, cell_size, sampler, height_cache);
        entry.origin = origin;
        entry.cell_size = cell_size;
        return entry;
    }

    chunk_storage chunk{extent};
    auto voxels = chunk.voxels();
    for (std::uint32_t z = 0; z < extent.z; ++z) {
        const double sample_z = static_cast<double>(origin[2]) + (static_cast<double>(z) + 0.5);
        const std::int64_t world_z = static_cast<std::int64_t>(std::floor(sample_z - 0.5));
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * cell_stride;
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                const double terrain = height_cache.cell[row + static_cast<std::size_t>(x)];
                if (cell_size == 1 && edits != nullptr) {
                    const std::int64_t world_x = origin[0] + static_cast<std::int64_t>(x);
                    const std::int64_t world_y = origin[1] + static_cast<std::int64_t>(y);
                    const std::int64_t world_z_block = origin[2] + static_cast<std::int64_t>(z);
                    voxels(x, y, z) = sample_voxel_with_overrides(
                        sampler, edits, world_x, world_y, world_z_block);
                } else {
                    voxels(x, y, z) = sampler.classify(sample_z, terrain, world_z);
                }
            }
        }
    }

    if (mode == mesher_choice::marching) {
        auto density_sampler = [&](std::size_t vx, std::size_t vy, std::size_t vz) {
            const double sample_z = static_cast<double>(origin[2]) + static_cast<double>(vz);
            const std::size_t index = vy * vertex_stride + vx;
            const double terrain = height_cache.vertex[index];
            float density = static_cast<float>(sample_z - terrain);
            if (cell_size == 1 && edits != nullptr) {
                const std::int64_t world_x = origin[0] + static_cast<std::int64_t>(vx);
                const std::int64_t world_y = origin[1] + static_cast<std::int64_t>(vy);
                const std::int64_t world_z = static_cast<std::int64_t>(std::floor(sample_z - 0.5));
                voxel_coord key{world_x, world_y, world_z};
                if (auto it = edits->overrides.find(key); it != edits->overrides.end()) {
                    density = (it->second != voxel_id{}) ? -0.25f : 0.25f;
                }
            }
            return density;
        };

        auto material_sampler = [voxels](std::size_t x, std::size_t y, std::size_t z) {
            return voxels(x, y, z);
        };

        meshing::marching_cubes_config config{};
        config.iso_value = 0.0f;
        entry.mesh = meshing::marching_cubes(extent, density_sampler, material_sampler, config);
    } else {
        auto is_opaque = [](voxel_id id) { return id != voxel_id{}; };
        auto neighbor_sampler = [&](const std::array<std::ptrdiff_t, 3>& coord) {
            if (coord[0] >= 0 && coord[0] < static_cast<std::ptrdiff_t>(extent.x)
                && coord[1] >= 0 && coord[1] < static_cast<std::ptrdiff_t>(extent.y)) {
                const auto x_index = static_cast<std::size_t>(coord[0]);
                const auto y_index = static_cast<std::size_t>(coord[1]);
                const std::size_t row = y_index * cell_stride;
                const double terrain = height_cache.cell[row + x_index];
                const double sample_z = static_cast<double>(origin[2]) + (static_cast<double>(coord[2]) + 0.5);
                const std::int64_t world_z = static_cast<std::int64_t>(std::floor(sample_z - 0.5));
                if (cell_size == 1) {
                    const std::int64_t world_x = origin[0] + coord[0];
                    const std::int64_t world_y = origin[1] + coord[1];
                    const std::int64_t world_z = origin[2] + coord[2];
                    return voxel_is_solid(sampler, edits, world_x, world_y, world_z);
                }
                return sampler.classify(sample_z, terrain, world_z) != voxel_id{};
            }
            return cell_is_solid(origin, cell_size, coord, sampler, edits);
        };
        entry.mesh = meshing::greedy_mesh_with_neighbors(chunk, is_opaque, neighbor_sampler);
    }
    entry.origin = origin;
    entry.cell_size = cell_size;

    return entry;
}

struct chunk_mesh_result {
    chunk_instance_key key{};
    chunk_mesh_entry entry{};
    std::uint64_t generation{};
};

class chunk_build_dispatcher {
public:
    chunk_build_dispatcher();
    ~chunk_build_dispatcher();

    void enqueue(const chunk_instance_key& key, const chunk_extent& extent,
        const std::array<std::int64_t, 3>& origin, int cell_size, mesher_choice mode,
        std::shared_ptr<const terrain_sampler> sampler,
        std::shared_ptr<const voxel_edit_state> edits);
    bool is_pending(const chunk_instance_key& key) const;
    void drain_ready(std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash>& cache);
    void clear();

private:
    struct build_job {
        chunk_extent extent{};
        std::array<std::int64_t, 3> origin{};
        int cell_size{1};
        mesher_choice mode{mesher_choice::greedy};
        std::uint64_t generation{0};
        std::shared_ptr<const terrain_sampler> sampler{};
        std::shared_ptr<const voxel_edit_state> edits{};
    };

    void worker_loop(std::stop_token stop_token);

    mutable std::mutex mutex_{};
    std::condition_variable cv_{};
    std::deque<chunk_instance_key> queue_{};
    std::unordered_map<chunk_instance_key, build_job, chunk_instance_hash> queued_jobs_{};
    std::unordered_map<chunk_instance_key, build_job, chunk_instance_hash> pending_jobs_{};
    std::unordered_map<chunk_instance_key, std::uint64_t, chunk_instance_hash> running_jobs_{};
    std::deque<chunk_mesh_result> ready_{};
    std::uint64_t generation_{0};
    std::jthread worker_{};
};

chunk_build_dispatcher::chunk_build_dispatcher()
    : worker_{[this](std::stop_token stop_token) { worker_loop(stop_token); }} {
}

chunk_build_dispatcher::~chunk_build_dispatcher() {
    clear();
    worker_.request_stop();
    cv_.notify_all();
}

void chunk_build_dispatcher::enqueue(const chunk_instance_key& key, const chunk_extent& extent,
    const std::array<std::int64_t, 3>& origin, int cell_size, mesher_choice mode,
    std::shared_ptr<const terrain_sampler> sampler,
    std::shared_ptr<const voxel_edit_state> edits) {
    std::scoped_lock lock{mutex_};
    build_job job{extent, origin, cell_size, mode, generation_, std::move(sampler), std::move(edits)};

    if (auto it = running_jobs_.find(key); it != running_jobs_.end()) {
        pending_jobs_.insert_or_assign(key, job);
        return;
    }

    if (auto it = queued_jobs_.find(key); it != queued_jobs_.end()) {
        it->second = job;
        return;
    }

    queue_.push_back(key);
    queued_jobs_.emplace(key, std::move(job));
    cv_.notify_one();
}

bool chunk_build_dispatcher::is_pending(const chunk_instance_key& key) const {
    std::scoped_lock lock{mutex_};

    if (auto it = queued_jobs_.find(key); it != queued_jobs_.end()) {
        if (it->second.generation == generation_) {
            return true;
        }
    }

    if (auto it = pending_jobs_.find(key); it != pending_jobs_.end()) {
        if (it->second.generation == generation_) {
            return true;
        }
    }

    if (auto it = running_jobs_.find(key); it != running_jobs_.end()) {
        if (it->second == generation_) {
            return true;
        }
    }

    return false;
}

void chunk_build_dispatcher::drain_ready(
    std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash>& cache) {
    std::deque<chunk_mesh_result> ready_local;
    {
        std::scoped_lock lock{mutex_};
        while (!ready_.empty()) {
            ready_local.push_back(std::move(ready_.front()));
            ready_.pop_front();
        }
    }

    while (!ready_local.empty()) {
        auto result = std::move(ready_local.front());
        ready_local.pop_front();
        if (result.generation != generation_) {
            continue;
        }
        cache[result.key] = std::move(result.entry);
    }
}

void chunk_build_dispatcher::clear() {
    std::scoped_lock lock{mutex_};
    ++generation_;
    queue_.clear();
    queued_jobs_.clear();
    pending_jobs_.clear();
    ready_.clear();
}

void chunk_build_dispatcher::worker_loop(std::stop_token stop_token) {
    while (true) {
        chunk_instance_key key{};
        build_job job{};

        {
            std::unique_lock lock{mutex_};
            cv_.wait(lock, [&] { return stop_token.stop_requested() || !queue_.empty(); });
            if (stop_token.stop_requested() && queue_.empty()) {
                return;
            }
            if (queue_.empty()) {
                continue;
            }

            key = queue_.front();
            queue_.pop_front();
            job = queued_jobs_.at(key);
            queued_jobs_.erase(key);
            running_jobs_[key] = job.generation;
        }

        terrain_sampler fallback{terrain_mode::smooth, job.extent};
        const terrain_sampler* sampler = job.sampler ? job.sampler.get() : &fallback;
        const voxel_edit_state* edit_ptr = nullptr;
        std::shared_lock<std::shared_mutex> edit_lock;
        if (job.edits) {
            edit_lock = std::shared_lock<std::shared_mutex>{job.edits->mutex};
            edit_ptr = job.edits.get();
        }
        auto entry = build_chunk_mesh(job.extent, job.origin, job.cell_size, job.mode, *sampler, edit_ptr);
        chunk_mesh_result result{key, std::move(entry), job.generation};

        {
            std::scoped_lock lock{mutex_};
            ready_.push_back(std::move(result));
            running_jobs_.erase(key);

            if (auto it = pending_jobs_.find(key); it != pending_jobs_.end()) {
                queue_.push_back(key);
                queued_jobs_[key] = it->second;
                pending_jobs_.erase(it);
                cv_.notify_one();
            }
        }
    }
}

void update_required_chunks(const camera& cam, const chunk_extent& extent, const std::array<lod_definition, 3>& lods,
    float render_distance_scale, mesher_choice mode, const std::shared_ptr<const terrain_sampler>& sampler,
    const std::shared_ptr<const voxel_edit_state>& edits,
    std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash>& cache,
    chunk_build_dispatcher& builder) {
    builder.drain_ready(cache);
    std::unordered_set<chunk_instance_key, chunk_instance_hash> needed;
    std::unordered_map<chunk_instance_key, std::array<std::int64_t, 3>, chunk_instance_hash> required_origins;
    std::unordered_map<chunk_instance_key, int, chunk_instance_hash> required_cell_sizes;
    const std::size_t build_budget = mode == mesher_choice::marching ? 8 : 12;
    std::size_t enqueued_this_frame = 0;
    const terrain_mode terrain_setting = sampler ? sampler->mode : terrain_mode::smooth;

    const double clamped_scale = std::clamp(static_cast<double>(render_distance_scale), 0.0, 10.0);

    for (const auto& lod : lods) {
        const double chunk_size = static_cast<double>(extent.x) * static_cast<double>(lod.cell_size);
        if (chunk_size <= 0.0) {
            continue;
        }

        const double base_min_distance = std::max(static_cast<double>(lod.min_distance), 0.0);
        const double scaled_max_distance_raw = static_cast<double>(lod.max_distance) * clamped_scale;
        double scaled_max_distance = std::max(scaled_max_distance_raw, 0.0);
        if (scaled_max_distance <= base_min_distance) {
            if (base_min_distance <= 0.0) {
                if (scaled_max_distance <= 0.0) {
                    continue;
                }
            } else {
                continue;
            }
        }

        const double scaled_min_distance = base_min_distance;
        if (scaled_max_distance <= 0.0) {
            continue;
        }

        const int base_x = static_cast<int>(std::floor(static_cast<double>(cam.position.x) / chunk_size));
        const int base_y = static_cast<int>(std::floor(static_cast<double>(cam.position.y) / chunk_size));
        const int radius = static_cast<int>(std::ceil(scaled_max_distance / chunk_size)) + 1;

        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                region_key region{base_x + dx, base_y + dy, 0};
                const std::array<std::int64_t, 3> origin{
                    static_cast<std::int64_t>(region.x) * static_cast<std::int64_t>(extent.x) * lod.cell_size,
                    static_cast<std::int64_t>(region.y) * static_cast<std::int64_t>(extent.y) * lod.cell_size,
                    static_cast<std::int64_t>(region.z) * static_cast<std::int64_t>(extent.z)
                };

                const double center_x = static_cast<double>(origin[0]) + chunk_size * 0.5;
                const double center_y = static_cast<double>(origin[1]) + chunk_size * 0.5;
                const double dx_world = center_x - static_cast<double>(cam.position.x);
                const double dy_world = center_y - static_cast<double>(cam.position.y);
                const double distance = std::sqrt(dx_world * dx_world + dy_world * dy_world);

                if (distance < scaled_min_distance || distance > scaled_max_distance) {
                    continue;
                }

                const chunk_instance_key key{region, lod.level};
                if (needed.insert(key).second) {
                    required_origins[key] = origin;
                    required_cell_sizes[key] = lod.cell_size;
                }

                auto it = cache.find(key);
                if (it == cache.end()) {
                    if (enqueued_this_frame >= build_budget || builder.is_pending(key)) {
                        continue;
                    }
                    builder.enqueue(key, extent, origin, lod.cell_size, mode, sampler, edits);
                    ++enqueued_this_frame;
                } else if (it->second.mode != mode || it->second.terrain != terrain_setting) {
                    if (builder.is_pending(key) || enqueued_this_frame >= build_budget) {
                        continue;
                    }
                    builder.enqueue(key, extent, origin, lod.cell_size, mode, sampler, edits);
                    ++enqueued_this_frame;
                }
            }
        }
    }

    const auto neighbor_present = [&](const chunk_instance_key& key, int dx, int dy) {
        chunk_instance_key neighbor_key{region_key{key.region.x + dx, key.region.y + dy, key.region.z}, key.lod};
        return cache.find(neighbor_key) != cache.end();
    };

    std::size_t requeued = 0;
    const std::size_t extra_budget = 2;
    for (const auto& [key, origin] : required_origins) {
        if (cache.find(key) != cache.end() || builder.is_pending(key)) {
            continue;
        }

        const bool horizontal_pair = neighbor_present(key, 1, 0) && neighbor_present(key, -1, 0);
        const bool vertical_pair = neighbor_present(key, 0, 1) && neighbor_present(key, 0, -1);
        if (!horizontal_pair && !vertical_pair) {
            continue;
        }

        if (enqueued_this_frame + requeued >= build_budget + extra_budget) {
            break;
        }

        const auto cell_size_it = required_cell_sizes.find(key);
        if (cell_size_it == required_cell_sizes.end()) {
            continue;
        }

        builder.enqueue(key, extent, origin, cell_size_it->second, mode, sampler, edits);
        ++requeued;
    }

    for (auto it = cache.begin(); it != cache.end();) {
        if (needed.contains(it->first)) {
            ++it;
        } else {
            it = cache.erase(it);
        }
    }
}

void rebuild_chunks_for_edit(const voxel_coord& block, const chunk_extent& extent,
    const std::array<lod_definition, 3>& lods, mesher_choice mode,
    const std::shared_ptr<const terrain_sampler>& sampler,
    const std::shared_ptr<voxel_edit_state>& edits,
    std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash>& cache,
    chunk_build_dispatcher& builder) {
    static constexpr std::array<std::pair<int, int>, 9> neighbor_offsets{
        std::to_array<std::pair<int, int>>({
            {0, 0},
            {1, 0},
            {-1, 0},
            {0, 1},
            {0, -1},
            {1, 1},
            {1, -1},
            {-1, 1},
            {-1, -1},
        })
    };

    for (const auto& lod : lods) {
        const std::int64_t span_x = static_cast<std::int64_t>(extent.x) * lod.cell_size;
        const std::int64_t span_y = static_cast<std::int64_t>(extent.y) * lod.cell_size;
        const std::int64_t region_x = floor_div(block.x, span_x);
        const std::int64_t region_y = floor_div(block.y, span_y);

        for (const auto& [dx, dy] : neighbor_offsets) {
            const std::int64_t rx = region_x + dx;
            const std::int64_t ry = region_y + dy;
            region_key region{static_cast<std::int32_t>(rx), static_cast<std::int32_t>(ry), 0};
            const chunk_instance_key key{region, lod.level};
            const std::array<std::int64_t, 3> origin{
                rx * span_x,
                ry * span_y,
                static_cast<std::int64_t>(region.z) * static_cast<std::int64_t>(extent.z)
            };
            builder.enqueue(key, extent, origin, lod.cell_size, mode, sampler, edits);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    using namespace almond::voxel;

    mesher_choice mesher_mode = mesher_choice::greedy;
    terrain_mode terrain_setting = terrain_mode::smooth;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--marching-cubes" || arg == "--mesher=marching") {
            mesher_mode = mesher_choice::marching;
        } else if (arg == "--mesher=greedy") {
            mesher_mode = mesher_choice::greedy;
        } else if (arg == "--terrain=classic" || arg == "--classic-terrain") {
            terrain_setting = terrain_mode::classic;
        } else if (arg == "--terrain=smooth") {
            terrain_setting = terrain_mode::smooth;
        }
    }

    try {
        if (test::has_registered_tests()) {
            test::run_tests();
        } else {
            std::cout << "No embedded unit tests registered; skipping test suite.\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Test suite failed: " << ex.what() << "\n";
        return 1;
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
    auto sampler = std::make_shared<terrain_sampler>(terrain_setting, chunk_dimensions);
    auto voxel_edits = std::make_shared<voxel_edit_state>();
    const std::array<lod_definition, 3> lods{
        lod_definition{0, 0.0f, 200.0f, 1},
        lod_definition{1, 180.0f, 480.0f, 2},
        lod_definition{2, 440.0f, 1400.0f, 4},
    };

    std::cout << "Starting terrain demo with "
              << (mesher_mode == mesher_choice::greedy ? "greedy mesher" : "marching cubes mesher")
              << " and "
              << (terrain_setting == terrain_mode::smooth ? "smooth noise terrain" : "classic heightfield terrain")
              << ". Toggle mesher with 'M' and terrain with 'T'. Left click removes voxels, right click places them."
              << " Press Space to jump and hold Shift to sprint. Use F3 to cycle debug overlays (wireframe, non-air, air-only).\n";
    std::cout << "Render distance starts at the minimum setting. Increase it gradually with '+' if performance allows.\n";

    const float3 player_half_extents{player_radius, player_radius, player_half_height};
    player_state player{};
    const double initial_height = sampler->height_at(0.0, -180.0);
    player.position = float3{0.0f, -180.0f, static_cast<float>(initial_height + static_cast<double>(player_half_height) + 2.0)};

    camera cam{};
    cam.yaw = 0.0f;
    cam.pitch = -0.45f;
    cam.position = add(player.position, float3{0.0f, 0.0f, player_eye_offset});

    auto sync_camera = [&]() {
        cam.position = add(player.position, float3{0.0f, 0.0f, player_eye_offset});
    };
    auto align_player_height = [&]() {
        const double height = sampler->height_at(player.position.x, player.position.y);
        player.position.z = static_cast<float>(height + static_cast<double>(player_half_height) + 1.0);
        {
            std::shared_lock<std::shared_mutex> lock{voxel_edits->mutex};
            const bool use_heightfield = collision_uses_heightfield(sampler->mode, mesher_mode);
            while (box_intersects_voxels(player.position, player_half_extents, *sampler, voxel_edits.get(), use_heightfield)) {
                player.position.z += 1.0f;
            }
        }
        player.velocity.z = 0.0f;
        player.on_ground = false;
        sync_camera();
    };
    align_player_height();

    bool running = true;
    debug_display_mode debug_mode = debug_display_mode::off;
    bool mouse_captured = true;
    std::uint64_t previous_ticks = SDL_GetTicks();
    std::uint64_t fps_sample_start = previous_ticks;
    int fps_frame_count = 0;
    float displayed_fps = 0.0f;

    std::unordered_map<chunk_instance_key, chunk_mesh_entry, chunk_instance_hash> chunk_meshes;
    chunk_build_dispatcher chunk_builder;

    std::optional<overlay_output> overlay_render_data;
    std::size_t overlay_generation = 0;

    constexpr float min_render_distance_scale = 0.25f;
    constexpr float max_render_distance_scale = 1.25f;
    constexpr float render_distance_step = 0.05f;
    float render_distance_scale = min_render_distance_scale;

    std::vector<projected_triangle> triangles;
    std::vector<SDL_Vertex> draw_vertices;
    std::vector<clip_vertex> clip_work;
    std::vector<clip_vertex> clip_temp;

    SDL_SetWindowRelativeMouseMode(window, true);

    while (running) {
        bool jump_requested = false;
        std::optional<voxel_coord> edited_block;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_F3) {
                    debug_mode = cycle_debug_mode(debug_mode, mesher_mode, terrain_setting);
                    std::cout << "Debug overlay: " << debug_mode_name(debug_mode) << "\n";
                } else if (event.key.key == SDLK_F1) {
                    mouse_captured = !mouse_captured;
                    SDL_SetWindowRelativeMouseMode(window, mouse_captured ? true : false);
                } else if (event.key.key == SDLK_M) {
                    mesher_mode = mesher_mode == mesher_choice::greedy ? mesher_choice::marching : mesher_choice::greedy;
                    chunk_meshes.clear();
                    chunk_builder.clear();
                    std::cout << "Switched mesher to "
                              << (mesher_mode == mesher_choice::greedy ? "greedy" : "marching cubes") << "\n";
                } else if (event.key.key == SDLK_T) {
                    terrain_setting = terrain_setting == terrain_mode::smooth ? terrain_mode::classic : terrain_mode::smooth;
                    sampler = std::make_shared<terrain_sampler>(terrain_setting, chunk_dimensions);
                    chunk_meshes.clear();
                    chunk_builder.clear();
                    std::cout << "Switched terrain to "
                              << (terrain_setting == terrain_mode::smooth ? "smooth noise" : "classic heightfield") << "\n";
                    align_player_height();
                } else if (event.key.key == SDLK_PLUS || event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS) {
                    const float previous_scale = render_distance_scale;
                    render_distance_scale = std::min(render_distance_scale + render_distance_step, max_render_distance_scale);
                    if (render_distance_scale != previous_scale) {
                        std::cout << "Render distance scale: "
                                  << static_cast<int>(std::lround(render_distance_scale * 100.0f)) << "%\n";
                    }
                } else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_UNDERSCORE
                    || event.key.key == SDLK_KP_MINUS) {
                    const float previous_scale = render_distance_scale;
                    render_distance_scale = std::max(render_distance_scale - render_distance_step, min_render_distance_scale);
                    if (render_distance_scale != previous_scale) {
                        std::cout << "Render distance scale: "
                                  << static_cast<int>(std::lround(render_distance_scale * 100.0f)) << "%\n";
                    }
                } else if (event.key.key == SDLK_SPACE) {
                    jump_requested = true;
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
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && mouse_captured) {
                if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
                    voxel_raycast_hit hit{};
                    {
                        std::shared_lock<std::shared_mutex> lock{voxel_edits->mutex};
                        const bool use_heightfield = collision_uses_heightfield(sampler->mode, mesher_mode);
                        hit = raycast_voxels(cam.position, compute_camera_vectors(cam).forward, 160.0f,
                            *sampler, voxel_edits.get(), use_heightfield);
                    }
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (hit.valid) {
                            std::unique_lock<std::shared_mutex> lock{voxel_edits->mutex};
                            const voxel_coord target = hit.hit;
                            if (is_bedrock_voxel(*sampler, voxel_edits.get(), target.x, target.y, target.z)) {
                                continue;
                            }
                            const voxel_id desired{};
                            const voxel_id base = sampler->voxel_at(hit.hit.x, hit.hit.y, hit.hit.z);
                            if (desired == base) {
                                voxel_edits->overrides.erase(hit.hit);
                            } else {
                                voxel_edits->overrides[hit.hit] = desired;
                            }
                            edited_block = hit.hit;
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        if (hit.valid && hit.has_previous) {
                            if (!block_intersects_player(hit.previous, player.position, player_half_extents)) {
                                std::unique_lock<std::shared_mutex> lock{voxel_edits->mutex};
                                const voxel_coord target = hit.previous;
                                if (is_bedrock_voxel(*sampler, voxel_edits.get(), target.x, target.y, target.z)) {
                                    continue;
                                }
                                const voxel_id desired{2};
                                const voxel_id base = sampler->voxel_at(hit.previous.x, hit.previous.y, hit.previous.z);
                                if (desired == base) {
                                    voxel_edits->overrides.erase(hit.previous);
                                } else {
                                    voxel_edits->overrides[hit.previous] = desired;
                                }
                                edited_block = hit.previous;
                            }
                        }
                    }
                }
            }
        }

        if (edited_block.has_value()) {
            rebuild_chunks_for_edit(*edited_block, chunk_dimensions, lods, mesher_mode,
                sampler, voxel_edits, chunk_meshes, chunk_builder);
        }

        const std::uint64_t current_ticks = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(current_ticks - previous_ticks) / 1000.0f;
        previous_ticks = current_ticks;

        ++fps_frame_count;
        const std::uint64_t fps_elapsed = current_ticks - fps_sample_start;
        if (fps_elapsed >= 250) {
            if (fps_elapsed > 0) {
                displayed_fps = static_cast<float>(fps_frame_count) * 1000.0f / static_cast<float>(fps_elapsed);
            }
            fps_frame_count = 0;
            fps_sample_start = current_ticks;
        }

        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);

        camera_vectors vectors = compute_camera_vectors(cam);

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        const float walk_speed = 8.0f;
        const float sprint_speed = 14.0f;
        const float move_speed = keyboard[SDL_SCANCODE_LSHIFT] ? sprint_speed : walk_speed;
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

        float3 desired_velocity{};
        if (length_squared(move_delta) > 1e-6f) {
            move_delta = normalize(move_delta);
            desired_velocity = scale(move_delta, move_speed);
        }
        player.velocity.x = desired_velocity.x;
        player.velocity.y = desired_velocity.y;

        const bool was_on_ground = player.on_ground;
        player.on_ground = false;

        if (jump_requested && was_on_ground) {
            player.velocity.z = jump_velocity;
        }

        player.velocity.z += gravity_acceleration * delta_seconds;
        player.velocity.z = std::max(player.velocity.z, -60.0f);

        float3 current_half_extents = player_half_extents;
        {
            std::shared_lock<std::shared_mutex> lock{voxel_edits->mutex};
            move_player_axis(player.position, player.velocity.x, player.velocity.x * delta_seconds, 0,
                current_half_extents, *sampler, voxel_edits.get(), mesher_mode, player.on_ground);
            move_player_axis(player.position, player.velocity.y, player.velocity.y * delta_seconds, 1,
                current_half_extents, *sampler, voxel_edits.get(), mesher_mode, player.on_ground);
            move_player_axis(player.position, player.velocity.z, player.velocity.z * delta_seconds, 2,
                current_half_extents, *sampler, voxel_edits.get(), mesher_mode, player.on_ground);
        }

        if (player.on_ground) {
            player.velocity.z = 0.0f;
        }

        const float player_bottom = player.position.z - current_half_extents.z;
        const float teleport_threshold = -static_cast<float>(bedrock_layer_count) - 1.0f;
        if (player_bottom < teleport_threshold) {
            std::optional<float3> recovery_position;
            {
                std::shared_lock<std::shared_mutex> lock{voxel_edits->mutex};
                recovery_position = find_sunlit_recovery_position(player.position, current_half_extents,
                    *sampler, voxel_edits.get(), mesher_mode);
            }
            if (recovery_position.has_value()) {
                player.position = *recovery_position;
                player.velocity = float3{};
                player.on_ground = false;
            }
        }

        sync_camera();

        vectors = compute_camera_vectors(cam);

        update_required_chunks(cam, chunk_dimensions, lods, render_distance_scale, mesher_mode, sampler, voxel_edits,
            chunk_meshes, chunk_builder);

        SDL_SetRenderDrawColor(renderer, 25, 25, 35, 255);
        SDL_RenderClear(renderer);

        std::size_t total_triangles = 0;
        for (const auto& [key, chunk] : chunk_meshes) {
            (void)key;
            total_triangles += chunk.mesh.indices.size() / 3;
        }

        triangles.clear();
        triangles.reserve(total_triangles * 2);
        draw_vertices.clear();
        draw_vertices.reserve(total_triangles * 6);
        clip_work.clear();
        clip_temp.clear();

        const auto world_position = [](const chunk_mesh_entry& chunk, const meshing::vertex& vertex) {
            return float3{
                static_cast<float>(chunk.origin[0]) + static_cast<float>(vertex.position[0]) * static_cast<float>(chunk.cell_size),
                static_cast<float>(chunk.origin[1]) + static_cast<float>(vertex.position[1]) * static_cast<float>(chunk.cell_size),
                static_cast<float>(chunk.origin[2]) + vertex.position[2]
            };
        };

        for (const auto& [key, chunk] : chunk_meshes) {
            const auto& mesh = chunk.mesh;
            std::uint32_t triangle_index = 0;
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

                clip_work.clear();
                clip_work.reserve(6);
                clip_temp.clear();
                clip_temp.reserve(6);

                const std::array<std::uint32_t, 3> indices{i0, i1, i2};
                const std::array<float3, 3> positions{p0, p1, p2};

                for (std::size_t corner = 0; corner < indices.size(); ++corner) {
                    const std::uint32_t idx = indices[corner];
                    const SDL_FColor color = shade_color(
                        mesh.vertices[idx].id, mesh.vertices[idx].normal, chunk.mode, chunk.terrain);
                    clip_work.push_back(make_clip_vertex(positions[corner], color, cam, vectors));
                }

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
                    const clip_vertex& v0 = clip_work[0];
                    const clip_vertex& v1 = clip_work[corner];
                    const clip_vertex& v2 = clip_work[corner + 1];

                    tri.vertices[0] = make_projected_vertex(v0, cam, output_width, output_height);
                    tri.vertices[1] = make_projected_vertex(v1, cam, output_width, output_height);
                    tri.vertices[2] = make_projected_vertex(v2, cam, output_width, output_height);
                    tri.camera_vertices[0] = float3{v0.x, v0.y, v0.z};
                    tri.camera_vertices[1] = float3{v1.x, v1.y, v1.z};
                    tri.camera_vertices[2] = float3{v2.x, v2.y, v2.z};
                    tri.depth = std::min({v0.z, v1.z, v2.z});
                    tri.region = key.region;
                    tri.lod = key.lod;
                    tri.sequence = triangle_index++;
                    tri.mesher = chunk.mode;
                    tri.normal = face_normal;
                    triangles.push_back(tri);
                }
            }
        }

        std::sort(triangles.begin(), triangles.end(), [](const projected_triangle& a, const projected_triangle& b) {
            const float depth_delta = a.depth - b.depth;
            if (std::abs(depth_delta) > 1e-5f) {
                return depth_delta > 0.0f;
            }
            if (a.lod != b.lod) {
                return a.lod < b.lod;
            }
            if (a.region.z != b.region.z) {
                return a.region.z < b.region.z;
            }
            if (a.region.y != b.region.y) {
                return a.region.y < b.region.y;
            }
            if (a.region.x != b.region.x) {
                return a.region.x < b.region.x;
            }
            return a.sequence < b.sequence;
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

        if (debug_mode != debug_display_mode::off) {
            overlay_input job{};
            job.mode = debug_mode;
            job.terrain = terrain_setting;
            job.cam = cam;
            job.vectors = vectors;
            job.output_width = output_width;
            job.output_height = output_height;
            job.triangles = triangles;
            job.chunks.reserve(chunk_meshes.size());
            for (const auto& [key, chunk] : chunk_meshes) {
                (void)key;
                chunk_overlay_info info{};
                info.base_x = static_cast<float>(chunk.origin[0]);
                info.base_y = static_cast<float>(chunk.origin[1]);
                info.base_z = static_cast<float>(chunk.origin[2]);
                info.width = static_cast<float>(chunk_dimensions.x * chunk.cell_size);
                info.depth = static_cast<float>(chunk_dimensions.y * chunk.cell_size);
                info.height = static_cast<float>(chunk_dimensions.z);
                info.is_air = chunk.mesh.indices.empty();
                job.chunks.push_back(info);
            }
            job.generation = ++overlay_generation;
            overlay_render_data = build_overlay_output(std::move(job));
        } else {
            overlay_render_data.reset();
        }

        if (overlay_render_data.has_value() && overlay_render_data->mode == debug_mode) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (const auto& group : overlay_render_data->groups) {
                SDL_SetRenderDrawColor(renderer, group.color.r, group.color.g, group.color.b, group.color.a);
                for (const auto& segment : group.segments) {
                    SDL_RenderLine(renderer, segment.start.x, segment.start.y, segment.end.x, segment.end.y);
                }
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        if (displayed_fps > 0.0f) {
            constexpr int fps_scale = 3;
            const std::string fps_text = [displayed_fps]() {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), "FPS: %.1f", displayed_fps);
                return std::string{buffer};
            }();
            const int text_width = measure_bitmap_text(fps_text, fps_scale);
            const int text_height = bitmap_font_height * fps_scale;
            const int padding = 4;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_FRect background{};
            background.x = static_cast<float>(padding);
            background.y = static_cast<float>(padding);
            background.w = static_cast<float>(text_width + padding * 2);
            background.h = static_cast<float>(text_height + padding * 2);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &background);
            draw_bitmap_text(renderer, fps_text, padding + 1, padding + 1, fps_scale, SDL_Color{255, 255, 255, 255});
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
