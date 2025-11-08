#include "almond_voxel/editing/voxel_editing.hpp"
#include "almond_voxel/effects/particle_emitter.hpp"
#include "almond_voxel/serialization/region_io.hpp"

#include <iostream>

using namespace almond::voxel;
using namespace almond::voxel::editing;
using namespace almond::voxel::effects;

int main() {
    region_manager regions{cubic_extent(8)};

    const world_position emitter_pos{0, 0, 0};
    particle_emitter_brush brush{};
    brush.density = 8.0f;
    brush.lifetime = 3.0f;
    brush.initial_velocity = velocity_sample{0.0f, 2.0f, 0.0f};

    decay_settings decay{};
    decay.delta_time = 0.5f;
    decay.velocity_damping = 0.85f;

    if (!paint_particle_emitter(regions, emitter_pos, brush, decay)) {
        std::cerr << "Failed to paint particle emitter" << std::endl;
        return 1;
    }

    const auto coords = split_world_position(emitter_pos, regions.chunk_dimensions());
    auto& chunk = regions.assure(coords.region);

    std::cout << "Simulating emitter decay" << std::endl;
    for (int step = 0; step < 10; ++step) {
        const auto processed = regions.tick(1);
        auto lifetime = chunk.effect_lifetime();
        auto density = chunk.effect_density();
        auto velocity = chunk.effect_velocity();
        const auto& sample_velocity = velocity(coords.local[0], coords.local[1], coords.local[2]);
        std::cout << "step " << step << ": tasks=" << processed << ", lifetime="
                  << lifetime(coords.local[0], coords.local[1], coords.local[2]) << ", density="
                  << density(coords.local[0], coords.local[1], coords.local[2]) << ", velocity="
                  << '(' << sample_velocity.x << ',' << sample_velocity.y << ',' << sample_velocity.z << ')' << std::endl;
        if (processed == 0) {
            break;
        }
    }

    const auto payload = serialization::serialize_chunk(chunk);
    const auto restored = serialization::deserialize_chunk(std::span<const std::byte>{payload.data(), payload.size()});
    auto restored_lifetime = restored.effect_lifetime();
    std::cout << "Restored lifetime sample: "
              << restored_lifetime(coords.local[0], coords.local[1], coords.local[2]) << std::endl;

    return 0;
}

