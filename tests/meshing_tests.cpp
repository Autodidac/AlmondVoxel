#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "almond_voxel/meshing/marching_cubes.hpp"
#include "almond_voxel/meshing/neighbors.hpp"
#include "test_framework.hpp"

#include "almond_voxel/chunk.hpp"

#include <array>
#include <cstddef>
#include <cmath>

using namespace almond::voxel;

TEST_CASE(greedy_mesher_single_voxel) {
    chunk_storage chunk{cubic_extent(3)};
    chunk.fill(voxel_id{});
    auto voxels = chunk.voxels();
    voxels(1, 1, 1) = voxel_id{42};

    const auto mesh = meshing::greedy_mesh(chunk);
    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);

    for (const auto index : mesh.indices) {
        CHECK(index < mesh.vertices.size());
    }
}

TEST_CASE(greedy_mesher_respects_chunk_neighbors) {
    const auto extent = cubic_extent(2);
    chunk_storage primary{extent};
    chunk_storage neighbor{extent};
    primary.fill(voxel_id{1});
    neighbor.fill(voxel_id{1});

    meshing::chunk_neighbors neighbors{};
    neighbors.pos_x = &neighbor;

    const auto mesh = meshing::greedy_mesh_with_neighbor_chunks(primary, neighbors);
    bool has_positive_x_face = false;
    for (const auto& vertex : mesh.vertices) {
        if (std::abs(vertex.normal[0] - 1.0f) < 1e-5f && std::abs(vertex.normal[1]) < 1e-5f
            && std::abs(vertex.normal[2]) < 1e-5f) {
            has_positive_x_face = true;
            break;
        }
    }

    CHECK_FALSE(has_positive_x_face);
}

TEST_CASE(marching_cubes_single_triangle) {
    const chunk_extent extent{1, 1, 1};
    const float field[2][2][2] = {
        {
            {0.0f, 1.0f},
            {1.0f, 1.0f}
        },
        {
            {1.0f, 1.0f},
            {1.0f, 1.0f}
        }
    };

    auto density = [&](std::size_t x, std::size_t y, std::size_t z) {
        return field[x][y][z];
    };

    meshing::marching_cubes_config config{};
    config.iso_value = 0.5f;
    const voxel_id material{7};
    const auto mesh = meshing::marching_cubes(extent, density,
        [material](std::size_t, std::size_t, std::size_t) { return material; }, config);

    REQUIRE(mesh.vertices.size() == 3);
    REQUIRE(mesh.indices.size() == 3);
    for (const auto& vertex : mesh.vertices) {
        CHECK(vertex.id == material);
    }
}

TEST_CASE(marching_cubes_triangle_orientation) {
    const chunk_extent extent{1, 1, 1};
    const float field[2][2][2] = {
        {
            {0.0f, 1.0f},
            {1.0f, 1.0f}
        },
        {
            {1.0f, 1.0f},
            {1.0f, 1.0f}
        }
    };

    auto density = [&](std::size_t x, std::size_t y, std::size_t z) {
        return field[x][y][z];
    };

    const auto mesh = meshing::marching_cubes(extent, density,
        [](std::size_t, std::size_t, std::size_t) { return voxel_id{1}; });

    REQUIRE(mesh.indices.size() == 3);
    const auto& v0 = mesh.vertices[mesh.indices[0]];
    const auto& v1 = mesh.vertices[mesh.indices[1]];
    const auto& v2 = mesh.vertices[mesh.indices[2]];

    const auto edge = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return std::array<float, 3>{
            b[0] - a[0],
            b[1] - a[1],
            b[2] - a[2]
        };
    };

    const auto cross = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return std::array<float, 3>{
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]
        };
    };

    const auto dot = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };

    const auto normal = cross(edge(v0.position, v2.position), edge(v0.position, v1.position));
    const std::array<float, 3> view{1.0f - v0.position[0], 1.0f - v0.position[1], 1.0f - v0.position[2]};

    CHECK(dot(normal, view) < 0.0f);
}

TEST_CASE(marching_cubes_from_chunk_binary) {
    chunk_storage chunk{cubic_extent(1)};
    auto voxels = chunk.voxels();
    voxels(0, 0, 0) = voxel_id{42};

    const auto mesh = meshing::marching_cubes_from_chunk(chunk);
    CHECK_FALSE(mesh.vertices.empty());
    CHECK_FALSE(mesh.indices.empty());
}

TEST_CASE(marching_cubes_respects_chunk_neighbors) {
    const auto extent = cubic_extent(2);
    chunk_storage primary{extent};
    chunk_storage neighbor{extent};
    primary.fill(voxel_id{1});
    neighbor.fill(voxel_id{1});

    meshing::chunk_neighbors neighbors{};
    neighbors.pos_x = &neighbor;

    const auto mesh = meshing::marching_cubes_from_chunk(primary,
        [](voxel_id id) { return id != voxel_id{}; }, neighbors);

    bool has_positive_x_surface = false;
    for (const auto& vertex : mesh.vertices) {
        if (vertex.normal[0] > 0.5f) {
            has_positive_x_surface = true;
            break;
        }
    }

    CHECK_FALSE(has_positive_x_surface);
}
