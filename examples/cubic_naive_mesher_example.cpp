#include "almond_voxel/chunk.hpp"
#include "almond_voxel/meshing/mesh_types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

using namespace almond::voxel;

namespace {

struct face_definition {
    std::array<std::array<float, 3>, 4> corners{};
    std::array<std::array<float, 2>, 4> uvs{};
};

constexpr std::array<face_definition, block_face_count> face_definitions{{
    face_definition{{{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    face_definition{{{{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    face_definition{{{{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
    face_definition{{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
        {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}}},
}};

[[nodiscard]] bool voxel_is_solid(span3d<const voxel_id> voxels, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0) {
        return false;
    }
    const auto ux = static_cast<std::size_t>(x);
    const auto uy = static_cast<std::size_t>(y);
    const auto uz = static_cast<std::size_t>(z);
    if (!voxels.contains(ux, uy, uz)) {
        return false;
    }
    return voxels(ux, uy, uz) != voxel_id{};
}

void emit_face(meshing::mesh_result& mesh, block_face face, float base_x, float base_y, float base_z, voxel_id id) {
    const auto& definition = face_definitions[static_cast<std::size_t>(face)];
    const auto normal = face_normal(face);
    const std::array<float, 3> normal_f{
        static_cast<float>(normal[0]), static_cast<float>(normal[1]), static_cast<float>(normal[2])};

    const std::uint32_t start_index = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::size_t i = 0; i < definition.corners.size(); ++i) {
        meshing::vertex vertex{};
        vertex.position = {
            base_x + definition.corners[i][0],
            base_y + definition.corners[i][1],
            base_z + definition.corners[i][2],
        };
        vertex.normal = normal_f;
        vertex.uv = definition.uvs[i];
        vertex.id = id;
        mesh.vertices.push_back(vertex);
    }

    mesh.indices.push_back(start_index + 0);
    mesh.indices.push_back(start_index + 1);
    mesh.indices.push_back(start_index + 2);
    mesh.indices.push_back(start_index + 0);
    mesh.indices.push_back(start_index + 2);
    mesh.indices.push_back(start_index + 3);
}

[[nodiscard]] meshing::mesh_result naive_cubic_mesh(const chunk_storage& chunk) {
    meshing::mesh_result mesh{};
    const auto voxels = chunk.voxels();
    const auto extent = chunk.extent();

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                const voxel_id id = voxels(x, y, z);
                if (id == voxel_id{}) {
                    continue;
                }

                for (block_face face : {block_face::pos_x, block_face::neg_x, block_face::pos_y, block_face::neg_y,
                         block_face::pos_z, block_face::neg_z}) {
                    const auto offset = face_normal(face);
                    const int neighbor_x = static_cast<int>(x) + offset[0];
                    const int neighbor_y = static_cast<int>(y) + offset[1];
                    const int neighbor_z = static_cast<int>(z) + offset[2];
                    if (!voxel_is_solid(voxels, neighbor_x, neighbor_y, neighbor_z)) {
                        emit_face(mesh, face, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), id);
                    }
                }
            }
        }
    }

    return mesh;
}

} // namespace

int main() {
    chunk_storage chunk{cubic_extent(32)};
    auto voxels = chunk.voxels();

    for (std::uint32_t z = 0; z < chunk.extent().z; ++z) {
        for (std::uint32_t y = 0; y < chunk.extent().y; ++y) {
            for (std::uint32_t x = 0; x < chunk.extent().x; ++x) {
                const bool stratified_layer = z < 6 || (z < 16 && ((x ^ y) & 0x3) == 0);
                const bool scattered_pillars = (x + y + z) % 9 == 0;
                if (stratified_layer || scattered_pillars) {
                    voxels(x, y, z) = voxel_id{1};
                } else {
                    voxels(x, y, z) = voxel_id{};
                }
            }
        }
    }

    const meshing::mesh_result mesh = naive_cubic_mesh(chunk);

    const std::size_t triangle_count = mesh.indices.size() / 3;
    const std::size_t quad_count = mesh.indices.size() / 6;
    const auto voxel_linear = chunk.voxels().linear();
    const std::size_t empty_voxels = static_cast<std::size_t>(
        std::count(voxel_linear.begin(), voxel_linear.end(), voxel_id{}));
    const std::size_t solid_voxels = voxel_linear.size() - empty_voxels;

    std::cout << "Generated naive cubic mesh (no greedy merging) from chunk of extent "
              << chunk.extent().x << "x" << chunk.extent().y << "x" << chunk.extent().z << "\n";
    std::cout << "  Solid voxels: " << solid_voxels << "\n";
    std::cout << "  Vertices:      " << mesh.vertices.size() << "\n";
    std::cout << "  Quads:         " << quad_count << "\n";
    std::cout << "  Triangles:     " << triangle_count << "\n";

    return 0;
}
