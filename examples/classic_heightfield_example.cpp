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

    terrain::classic_heightfield generator{cubic_extent(32), config, 20240522ull};

    const region_key origin{0, 0, 0};
    chunk_storage chunk = generator(origin);

    std::map<voxel_id, std::size_t> histogram{};
    const auto voxels = chunk.voxels();
    const auto extent = chunk.extent();

    for (std::uint32_t z = 0; z < extent.z; ++z) {
        for (std::uint32_t y = 0; y < extent.y; ++y) {
            for (std::uint32_t x = 0; x < extent.x; ++x) {
                ++histogram[voxels(x, y, z)];
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

    return 0;
}
