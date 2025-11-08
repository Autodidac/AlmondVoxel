#include "almond_voxel/terrain/classic.hpp"

#include <cstdint>
#include <iostream>
#include <map>

int main() {
    using namespace almond::voxel;

    terrain::classic_config config{};
    config.surface_voxel = voxel_id{2};
    config.filler_voxel = voxel_id{3};
    config.subsurface_voxel = voxel_id{4};
    config.bedrock_voxel = voxel_id{5};
    config.bedrock_layers = 3;
    config.surface_depth = 3;
    config.surface_material = material_index{11};
    config.filler_material = material_index{12};
    config.subsurface_material = material_index{13};
    config.bedrock_material = material_index{14};
    config.air_material = null_material_index;

    terrain::classic_heightfield generator{cubic_extent(32), config, 20240522ull};

    const region_key origin{0, 0, 0};
    chunk_storage chunk = generator(origin);

    std::map<voxel_id, std::size_t> histogram{};
    std::map<material_index, std::size_t> material_histogram{};
    const auto voxels = chunk.voxels();
    const auto materials = chunk.materials();
    const auto extent = chunk.extent();

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                ++histogram[voxels(x, y, z)];
                ++material_histogram[materials(x, y, z)];
            }
        }
    }

    const std::size_t total_voxels = static_cast<std::size_t>(extent.volume());

    std::cout << "Generated classic heightfield chunk (" << extent.x << "x" << extent.y << "x" << extent.z
              << ") with " << total_voxels << " voxels.\n";
    std::cout << "Voxel distribution:\n";
    for (const auto& [id, count] : histogram) {
        const double percentage = total_voxels == 0 ? 0.0
            : static_cast<double>(count) * 100.0 / static_cast<double>(total_voxels);
        std::cout << "  id=" << static_cast<unsigned int>(id)
                  << " -> " << count << " voxels ("
                  << percentage << "%)\n";
    }

    std::cout << "Material distribution:\n";
    for (const auto& [id, count] : material_histogram) {
        const double percentage = total_voxels == 0 ? 0.0
            : static_cast<double>(count) * 100.0 / static_cast<double>(total_voxels);
        std::cout << "  material=" << id << " -> " << count << " assignments ("
                  << percentage << "%)\n";
    }

    return 0;
}
