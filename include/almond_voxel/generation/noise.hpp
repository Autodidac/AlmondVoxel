#pragma once

#include "almond_voxel/core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace almond::voxel::generation {

class value_noise {
public:
    explicit value_noise(std::uint64_t seed = 0, double frequency = 1.0, std::size_t octaves = 4, double persistence = 0.5) noexcept
        : seed_{seed}
        , frequency_{frequency}
        , octaves_{octaves}
        , persistence_{persistence} {
    }

    [[nodiscard]] double sample(double x, double y, double z = 0.0) const noexcept {
        double amplitude = 1.0;
        double frequency = frequency_;
        double sum = 0.0;
        double max_amplitude = 0.0;
        for (std::size_t octave = 0; octave < octaves_; ++octave) {
            sum += amplitude * gradient_noise(x * frequency, y * frequency, z * frequency);
            max_amplitude += amplitude;
            amplitude *= persistence_;
            frequency *= 2.0;
        }
        if (max_amplitude == 0.0) {
            return 0.0;
        }
        return sum / max_amplitude;
    }

private:
    [[nodiscard]] double gradient_noise(double x, double y, double z) const noexcept {
        const auto xi = static_cast<std::int64_t>(std::floor(x));
        const auto yi = static_cast<std::int64_t>(std::floor(y));
        const auto zi = static_cast<std::int64_t>(std::floor(z));
        const double xf = x - static_cast<double>(xi);
        const double yf = y - static_cast<double>(yi);
        const double zf = z - static_cast<double>(zi);

        const double u = fade(xf);
        const double v = fade(yf);
        const double w = fade(zf);

        const double c000 = grad(hash(xi, yi, zi), xf, yf, zf);
        const double c100 = grad(hash(xi + 1, yi, zi), xf - 1.0, yf, zf);
        const double c010 = grad(hash(xi, yi + 1, zi), xf, yf - 1.0, zf);
        const double c110 = grad(hash(xi + 1, yi + 1, zi), xf - 1.0, yf - 1.0, zf);
        const double c001 = grad(hash(xi, yi, zi + 1), xf, yf, zf - 1.0);
        const double c101 = grad(hash(xi + 1, yi, zi + 1), xf - 1.0, yf, zf - 1.0);
        const double c011 = grad(hash(xi, yi + 1, zi + 1), xf, yf - 1.0, zf - 1.0);
        const double c111 = grad(hash(xi + 1, yi + 1, zi + 1), xf - 1.0, yf - 1.0, zf - 1.0);

        const double x00 = lerp(c000, c100, u);
        const double x10 = lerp(c010, c110, u);
        const double x01 = lerp(c001, c101, u);
        const double x11 = lerp(c011, c111, u);

        const double y0 = lerp(x00, x10, v);
        const double y1 = lerp(x01, x11, v);

        return lerp(y0, y1, w);
    }

    [[nodiscard]] static constexpr double fade(double t) noexcept {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    [[nodiscard]] static constexpr double lerp(double a, double b, double t) noexcept {
        return a + t * (b - a);
    }

    [[nodiscard]] double grad(std::uint64_t hash_value, double x, double y, double z) const noexcept {
        const std::uint64_t h = hash_value & 15ull;
        const double u = h < 8ull ? x : y;
        const double v = h < 4ull ? y : (h == 12ull || h == 14ull ? x : z);
        const double first = (h & 1ull) == 0ull ? u : -u;
        const double second = (h & 2ull) == 0ull ? v : -v;
        return first + second;
    }

    [[nodiscard]] std::uint64_t hash(std::int64_t x, std::int64_t y, std::int64_t z) const noexcept {
        std::uint64_t h = seed_;
        h ^= static_cast<std::uint64_t>(x) * 0x9E3779B185EBCA87ull;
        h ^= static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full;
        h ^= static_cast<std::uint64_t>(z) * 0x165667B19E3779F9ull;
        h ^= (h >> 33);
        h *= 0xff51afd7ed558ccdull;
        h ^= (h >> 33);
        h *= 0xc4ceb9fe1a85ec53ull;
        h ^= (h >> 33);
        return h;
    }

    std::uint64_t seed_;
    double frequency_;
    std::size_t octaves_;
    double persistence_;
};

struct palette_entry {
    double threshold{0.0};
    voxel_id id{0};
};

class palette_builder {
public:
    palette_builder& add(double threshold, voxel_id id) {
        entries_.push_back(palette_entry{threshold, id});
        std::sort(entries_.begin(), entries_.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.threshold < rhs.threshold;
        });
        return *this;
    }

    [[nodiscard]] voxel_id choose(double value) const noexcept {
        if (entries_.empty()) {
            return voxel_id{};
        }
        for (const auto& entry : entries_) {
            if (value <= entry.threshold) {
                return entry.id;
            }
        }
        return entries_.back().id;
    }

    [[nodiscard]] const std::vector<palette_entry>& entries() const noexcept { return entries_; }

private:
    std::vector<palette_entry> entries_{};
};

[[nodiscard]] inline double remap(double value, double min, double max) noexcept {
    if (min == max) {
        return 0.0;
    }
    const double clamped = std::clamp(value, min, max);
    return (clamped - min) / (max - min);
}

template <typename OutputIt>
void sample_heightmap(const value_noise& noise, const chunk_extent& extent, double scale, OutputIt out) {
    const auto dims = extent.to_array();
    for (std::uint32_t z = 0; z < dims[2]; ++z) {
        for (std::uint32_t x = 0; x < dims[0]; ++x) {
            const double nx = static_cast<double>(x) / static_cast<double>(dims[0]);
            const double nz = static_cast<double>(z) / static_cast<double>(dims[2]);
            *out++ = noise.sample(nx * scale, nz * scale);
        }
    }
}

template <typename Range>
[[nodiscard]] std::vector<voxel_id> build_palette(const Range& samples, const palette_builder& palette) {
    std::vector<voxel_id> result;
    result.reserve(samples.size());
    if (samples.empty()) {
        return result;
    }
    const auto [min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
    const double min_v = *min_it;
    const double max_v = *max_it;
    for (const auto& value : samples) {
        result.push_back(palette.choose(remap(value, min_v, max_v)));
    }
    return result;
}

} // namespace almond::voxel::generation
