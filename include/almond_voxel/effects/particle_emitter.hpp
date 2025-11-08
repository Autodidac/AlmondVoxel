#pragma once

#include "almond_voxel/chunk.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace almond::voxel::effects {

struct particle_emitter_brush {
    float density{1.0f};
    float lifetime{1.0f};
    velocity_sample initial_velocity{};
};

struct decay_settings {
    float delta_time{1.0f};
    float velocity_damping{0.95f};
};

inline bool stamp_emitter(chunk_storage& chunk, const std::array<std::uint32_t, 3>& local,
    const particle_emitter_brush& brush) {
    if (!chunk.effect_density_enabled() || !chunk.effect_velocity_enabled() || !chunk.effect_lifetime_enabled()) {
        return false;
    }

    auto density = chunk.effect_density();
    if (!density.contains(local[0], local[1], local[2])) {
        return false;
    }

    auto lifetime = chunk.effect_lifetime();
    auto velocity = chunk.effect_velocity();

    density(local[0], local[1], local[2]) = brush.density;
    lifetime(local[0], local[1], local[2]) = brush.lifetime;
    velocity(local[0], local[1], local[2]) = brush.initial_velocity;
    return true;
}

inline bool has_active_effects(const chunk_storage& chunk) {
    if (!chunk.effect_lifetime_enabled()) {
        return false;
    }
    auto lifetime = chunk.effect_lifetime();
    for (const float value : lifetime.linear()) {
        if (value > 0.0f) {
            return true;
        }
    }
    return false;
}

inline bool simulate_decay(chunk_storage& chunk, decay_settings settings) {
    if (!chunk.effect_lifetime_enabled()) {
        return false;
    }

    auto lifetime = chunk.effect_lifetime();
    auto lifetime_linear = lifetime.linear();

    voxel_span<float> density_linear{};
    if (chunk.effect_density_enabled()) {
        density_linear = chunk.effect_density().linear();
    }

    voxel_span<velocity_sample> velocity_linear{};
    if (chunk.effect_velocity_enabled()) {
        velocity_linear = chunk.effect_velocity().linear();
    }

    bool any_alive = false;
    const auto count = lifetime_linear.size();
    for (std::size_t i = 0; i < count; ++i) {
        float& life = lifetime_linear[i];
        if (life <= 0.0f) {
            if (!density_linear.empty()) {
                density_linear[i] = 0.0f;
            }
            if (!velocity_linear.empty()) {
                velocity_linear[i] = velocity_sample{};
            }
            continue;
        }

        life = std::max(0.0f, life - settings.delta_time);
        if (life > 0.0f) {
            any_alive = true;
            if (!velocity_linear.empty()) {
                velocity_sample& vel = velocity_linear[i];
                vel.x *= settings.velocity_damping;
                vel.y *= settings.velocity_damping;
                vel.z *= settings.velocity_damping;
            }
        } else {
            if (!density_linear.empty()) {
                density_linear[i] = 0.0f;
            }
            if (!velocity_linear.empty()) {
                velocity_linear[i] = velocity_sample{};
            }
        }
    }

    return any_alive;
}

} // namespace almond::voxel::effects

