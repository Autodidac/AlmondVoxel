#include "almond_voxel/meshing/greedy_mesher.hpp"
#include "almond_voxel/meshing/marching_cubes.hpp"
#include "almond_voxel/meshing/neighbors.hpp"
#include "test_framework.hpp"

#include "almond_voxel/chunk.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <tuple>
#include <vector>

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

TEST_CASE(greedy_mesher_respects_vertical_neighbors) {
    const auto extent = cubic_extent(2);
    chunk_storage primary{extent};
    chunk_storage above{extent};
    chunk_storage below{extent};
    primary.fill(voxel_id{1});
    above.fill(voxel_id{1});
    below.fill(voxel_id{1});

    meshing::chunk_neighbors neighbors{};
    neighbors.pos_z = &above;

    const auto mesh_with_above = meshing::greedy_mesh_with_neighbor_chunks(primary, neighbors);
    bool has_positive_z_face = false;
    for (const auto& vertex : mesh_with_above.vertices) {
        if (std::abs(vertex.normal[2] - 1.0f) < 1e-5f && std::abs(vertex.normal[0]) < 1e-5f
            && std::abs(vertex.normal[1]) < 1e-5f) {
            has_positive_z_face = true;
            break;
        }
    }
    CHECK_FALSE(has_positive_z_face);

    meshing::chunk_neighbors below_neighbor{};
    below_neighbor.neg_z = &below;

    const auto mesh_with_below = meshing::greedy_mesh_with_neighbor_chunks(primary, below_neighbor);
    bool has_negative_z_face = false;
    for (const auto& vertex : mesh_with_below.vertices) {
        if (std::abs(vertex.normal[2] + 1.0f) < 1e-5f && std::abs(vertex.normal[0]) < 1e-5f
            && std::abs(vertex.normal[1]) < 1e-5f) {
            has_negative_z_face = true;
            break;
        }
    }
    CHECK_FALSE(has_negative_z_face);

    meshing::chunk_neighbors both_neighbors{};
    both_neighbors.pos_z = &above;
    both_neighbors.neg_z = &below;

    const auto mesh_with_both = meshing::greedy_mesh_with_neighbor_chunks(primary, both_neighbors);
    bool has_any_vertical_face = false;
    for (const auto& vertex : mesh_with_both.vertices) {
        if (std::abs(vertex.normal[2]) > 1e-5f && std::abs(vertex.normal[0]) < 1e-5f
            && std::abs(vertex.normal[1]) < 1e-5f) {
            has_any_vertical_face = true;
            break;
        }
    }
    CHECK_FALSE(has_any_vertical_face);
}

TEST_CASE(greedy_mesher_emits_unique_vertical_faces) {
    const chunk_extent extent{3, 3, 3};
    chunk_storage chunk{extent};
    chunk.fill(voxel_id{});

    auto voxels = chunk.voxels();
    for (std::size_t y = 0; y < extent.y; ++y) {
        for (std::size_t x = 0; x < extent.x; ++x) {
            voxels(x, y, 0) = voxel_id{1};
            voxels(x, y, 1) = voxel_id{1};
        }
    }

    const auto mesh = meshing::greedy_mesh(chunk);

    std::vector<std::array<std::array<float, 3>, 3>> vertical_triangles;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto i0 = mesh.indices[i];
        const auto i1 = mesh.indices[i + 1];
        const auto i2 = mesh.indices[i + 2];
        const auto& v0 = mesh.vertices[i0];
        const auto& v1 = mesh.vertices[i1];
        const auto& v2 = mesh.vertices[i2];

        if (std::abs(v0.normal[2]) < 1e-5f || std::abs(v1.normal[2]) < 1e-5f || std::abs(v2.normal[2]) < 1e-5f) {
            continue;
        }

        std::array<std::array<float, 3>, 3> positions{v0.position, v1.position, v2.position};
        std::sort(positions.begin(), positions.end(), [](const auto& a, const auto& b) {
            if (a[0] != b[0]) {
                return a[0] < b[0];
            }
            if (a[1] != b[1]) {
                return a[1] < b[1];
            }
            return a[2] < b[2];
        });
        vertical_triangles.push_back(positions);
    }

    std::sort(vertical_triangles.begin(), vertical_triangles.end());
    const auto duplicate = std::adjacent_find(vertical_triangles.begin(), vertical_triangles.end());
    CHECK(duplicate == vertical_triangles.end());
}

TEST_CASE(greedy_mesher_avoids_vertical_overlap_across_chunks) {
    const chunk_extent extent{2, 2, 2};
    chunk_storage lower{extent};
    chunk_storage upper{extent};
    lower.fill(voxel_id{1});
    upper.fill(voxel_id{1});

    meshing::chunk_neighbors lower_neighbors{};
    lower_neighbors.pos_z = &upper;
    const auto lower_mesh = meshing::greedy_mesh_with_neighbor_chunks(lower, lower_neighbors);

    meshing::chunk_neighbors upper_neighbors{};
    upper_neighbors.neg_z = &lower;
    const auto upper_mesh = meshing::greedy_mesh_with_neighbor_chunks(upper, upper_neighbors);

    std::vector<std::tuple<float, float, float, int>> vertical_faces;
    const auto collect_faces = [&](const meshing::mesh_result& mesh, float base_z) {
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto i0 = mesh.indices[i];
            const auto i1 = mesh.indices[i + 1];
            const auto i2 = mesh.indices[i + 2];
            const auto& v0 = mesh.vertices[i0];
            const auto& v1 = mesh.vertices[i1];
            const auto& v2 = mesh.vertices[i2];

            const auto normal = v0.normal;
            if (std::abs(normal[2]) < 1e-5f) {
                continue;
            }

            const float z0 = base_z + v0.position[2];
            const float z1 = base_z + v1.position[2];
            const float z2 = base_z + v2.position[2];

            const float min_z = std::min({z0, z1, z2});
            const float max_z = std::max({z0, z1, z2});
            vertical_faces.emplace_back(min_z, max_z, normal[2], normal[2] > 0.0f ? 1 : -1);
        }
    };

    collect_faces(lower_mesh, 0.0f);
    collect_faces(upper_mesh, static_cast<float>(extent.z));

    std::sort(vertical_faces.begin(), vertical_faces.end(), [](const auto& a, const auto& b) {
        if (std::abs(std::get<0>(a) - std::get<0>(b)) > 1e-5f) {
            return std::get<0>(a) < std::get<0>(b);
        }
        if (std::abs(std::get<1>(a) - std::get<1>(b)) > 1e-5f) {
            return std::get<1>(a) < std::get<1>(b);
        }
        return std::get<3>(a) < std::get<3>(b);
    });

    const auto overlap = std::adjacent_find(vertical_faces.begin(), vertical_faces.end(), [](const auto& a, const auto& b) {
        const auto [amin, amax, _, a_sign] = a;
        const auto [bmin, bmax, __, b_sign] = b;
        return std::abs(amin - bmin) < 1e-5f && std::abs(amax - bmax) < 1e-5f && a_sign != b_sign;
    });

    CHECK(overlap == vertical_faces.end());
}

TEST_CASE(greedy_mesher_applies_vertical_face_bias) {
    const chunk_extent extent{1, 1, 1};
    chunk_storage chunk{extent};
    auto voxels = chunk.voxels();
    voxels(0, 0, 0) = voxel_id{1};

    const auto mesh = meshing::greedy_mesh(chunk);

    bool has_upper = false;
    bool has_lower = false;
    for (const auto& vertex : mesh.vertices) {
        if (std::abs(vertex.normal[2] - 1.0f) < 1e-5f) {
            CHECK(vertex.position[2] > 1.0f);
            CHECK(vertex.position[2] < 1.1f);
            has_upper = true;
        }
        if (std::abs(vertex.normal[2] + 1.0f) < 1e-5f) {
            CHECK(vertex.position[2] < 0.0f);
            CHECK(vertex.position[2] > -0.1f);
            has_lower = true;
        }
    }

    CHECK(has_upper);
    CHECK(has_lower);
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
