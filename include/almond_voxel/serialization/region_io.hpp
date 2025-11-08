#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace almond::voxel::serialization {

constexpr std::uint32_t chunk_version_latest = 2;
constexpr std::array<char, 4> chunk_magic{'A', 'V', 'C', 'K'};

struct chunk_header_v1 {
    char magic[4]{chunk_magic[0], chunk_magic[1], chunk_magic[2], chunk_magic[3]};
    std::uint32_t version{1};
    std::uint32_t extent[3]{1, 1, 1};
};

struct chunk_header_v2 {
    char magic[4]{chunk_magic[0], chunk_magic[1], chunk_magic[2], chunk_magic[3]};
    std::uint32_t version{chunk_version_latest};
    std::uint32_t extent[3]{1, 1, 1};
    std::uint32_t channel_flags{0};
};

enum chunk_channel_flags : std::uint32_t {
    chunk_channel_materials = 1u << 0u,
    chunk_channel_skylight_cache = 1u << 1u,
    chunk_channel_blocklight_cache = 1u << 2u
};

struct region_blob {
    region_key key{};
    std::vector<std::byte> payload;
};

inline void append_bytes(std::vector<std::byte>& buffer, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

inline std::vector<std::byte> serialize_chunk(const chunk_storage& chunk) {
    const auto extent = chunk.extent();
    const auto voxel_data = chunk.voxels();
    const auto sky_data = chunk.skylight();
    const auto block_data = chunk.blocklight();
    const auto meta_data = chunk.metadata();
    const bool has_materials = chunk.materials_enabled();
    const bool has_high_precision = chunk.high_precision_lighting_enabled();

    chunk_header_v2 header{};
    header.extent[0] = extent.x;
    header.extent[1] = extent.y;
    header.extent[2] = extent.z;
    if (has_materials) {
        header.channel_flags |= chunk_channel_materials;
    }
    if (has_high_precision) {
        header.channel_flags |= chunk_channel_skylight_cache | chunk_channel_blocklight_cache;
    }

    const auto volume = extent.volume();
    std::size_t payload_bytes = volume * (sizeof(voxel_id) + 3);
    if (has_materials) {
        payload_bytes += volume * sizeof(material_index);
    }
    if (has_high_precision) {
        payload_bytes += volume * sizeof(float) * 2;
    }

    std::vector<std::byte> buffer;
    buffer.reserve(sizeof(chunk_header_v2) + payload_bytes);
    append_bytes(buffer, &header, sizeof(header));

    const auto copy_span = [&buffer](auto span) {
        using value_type = typename decltype(span)::value_type;
        append_bytes(buffer, span.data(), span.size() * sizeof(value_type));
    };

    copy_span(voxel_data.linear());
    copy_span(sky_data.linear());
    copy_span(block_data.linear());
    copy_span(meta_data.linear());

    if (has_materials) {
        copy_span(chunk.materials().linear());
    }
    if (has_high_precision) {
        copy_span(chunk.skylight_cache().linear());
        copy_span(chunk.blocklight_cache().linear());
    }

    return buffer;
}

inline chunk_storage deserialize_chunk(std::span<const std::byte> bytes) {
    if (bytes.size() < sizeof(chunk_header_v1)) {
        throw std::runtime_error("chunk payload too small");
    }

    chunk_header_v1 header_v1{};
    std::memcpy(&header_v1, bytes.data(), sizeof(header_v1));
    if (std::string_view(header_v1.magic, 4) != std::string_view{chunk_magic.data(), chunk_magic.size()}) {
        throw std::runtime_error("invalid chunk magic");
    }

    if (header_v1.version == 1) {
        const chunk_extent extent{header_v1.extent[0], header_v1.extent[1], header_v1.extent[2]};
        const auto count = extent.volume();
        const std::size_t required = sizeof(chunk_header_v1) + count * (sizeof(voxel_id) + 3);
        if (bytes.size() < required) {
            throw std::runtime_error("chunk payload truncated");
        }

        chunk_storage_config config{};
        config.extent = extent;
        chunk_storage chunk{config};
        const auto* ptr = bytes.data() + sizeof(chunk_header_v1);

        auto copy_into = [&ptr, count](auto view) {
            using value_type = typename decltype(view)::element_type;
            std::memcpy(view.linear().data(), ptr, count * sizeof(value_type));
            ptr += count * sizeof(value_type);
        };

        copy_into(chunk.voxels());
        copy_into(chunk.skylight());
        copy_into(chunk.blocklight());
        copy_into(chunk.metadata());
        chunk.mark_dirty(false);
        return chunk;
    }

    if (bytes.size() < sizeof(chunk_header_v2)) {
        throw std::runtime_error("chunk payload too small for extended header");
    }

    chunk_header_v2 header_v2{};
    std::memcpy(&header_v2, bytes.data(), sizeof(header_v2));
    if (header_v2.version < 2) {
        throw std::runtime_error("unsupported chunk version");
    }

    const chunk_extent extent{header_v2.extent[0], header_v2.extent[1], header_v2.extent[2]};
    const auto count = extent.volume();
    const bool has_materials = (header_v2.channel_flags & chunk_channel_materials) != 0;
    const bool has_sky_cache = (header_v2.channel_flags & chunk_channel_skylight_cache) != 0;
    const bool has_block_cache = (header_v2.channel_flags & chunk_channel_blocklight_cache) != 0;

    std::size_t required = sizeof(chunk_header_v2) + count * (sizeof(voxel_id) + 3);
    if (has_materials) {
        required += count * sizeof(material_index);
    }
    if (has_sky_cache) {
        required += count * sizeof(float);
    }
    if (has_block_cache) {
        required += count * sizeof(float);
    }
    if (bytes.size() < required) {
        throw std::runtime_error("chunk payload truncated");
    }

    chunk_storage_config config{};
    config.extent = extent;
    config.enable_materials = has_materials;
    config.enable_high_precision_lighting = has_sky_cache || has_block_cache;

    chunk_storage chunk{config};
    const auto* ptr = bytes.data() + sizeof(chunk_header_v2);

    auto copy_into = [&ptr, count](auto view) {
        using value_type = typename decltype(view)::element_type;
        std::memcpy(view.linear().data(), ptr, count * sizeof(value_type));
        ptr += count * sizeof(value_type);
    };

    copy_into(chunk.voxels());
    copy_into(chunk.skylight());
    copy_into(chunk.blocklight());
    copy_into(chunk.metadata());

    if (has_materials) {
        auto materials = chunk.materials();
        std::memcpy(materials.linear().data(), ptr, count * sizeof(material_index));
        ptr += count * sizeof(material_index);
    }

    if (config.enable_high_precision_lighting) {
        if (has_sky_cache) {
            auto sky_cache = chunk.skylight_cache();
            std::memcpy(sky_cache.linear().data(), ptr, count * sizeof(float));
            ptr += count * sizeof(float);
        }
        if (has_block_cache) {
            auto block_cache = chunk.blocklight_cache();
            std::memcpy(block_cache.linear().data(), ptr, count * sizeof(float));
            ptr += count * sizeof(float);
        }
    }

    chunk.mark_dirty(false);
    return chunk;
}

inline bool is_legacy_chunk_payload(std::span<const std::byte> bytes) {
    if (bytes.size() < sizeof(chunk_header_v1)) {
        return false;
    }
    chunk_header_v1 header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    return std::string_view(header.magic, 4) == std::string_view{chunk_magic.data(), chunk_magic.size()}
        && header.version == 1;
}

inline std::vector<std::byte> migrate_legacy_chunk_payload(std::span<const std::byte> bytes) {
    if (!is_legacy_chunk_payload(bytes)) {
        throw std::runtime_error("chunk payload is not a legacy format");
    }
    chunk_storage chunk = deserialize_chunk(bytes);
    return serialize_chunk(chunk);
}

inline void serialize_chunk_to_stream(const chunk_storage& chunk, std::ostream& out) {
    auto payload = serialize_chunk(chunk);
    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
}

inline chunk_storage deserialize_chunk_from_stream(std::istream& in) {
    chunk_header_v1 header_v1{};
    in.read(reinterpret_cast<char*>(&header_v1), sizeof(header_v1));
    if (!in) {
        throw std::runtime_error("unable to read chunk header");
    }
    if (std::string_view(header_v1.magic, 4) != std::string_view{chunk_magic.data(), chunk_magic.size()}) {
        throw std::runtime_error("invalid chunk magic");
    }

    if (header_v1.version == 1) {
        const chunk_extent extent{header_v1.extent[0], header_v1.extent[1], header_v1.extent[2]};
        const auto count = extent.volume();
        std::vector<std::byte> payload(sizeof(chunk_header_v1) + count * (sizeof(voxel_id) + 3));
        std::memcpy(payload.data(), &header_v1, sizeof(header_v1));
        in.read(reinterpret_cast<char*>(payload.data() + sizeof(header_v1)),
            static_cast<std::streamsize>(payload.size() - sizeof(header_v1)));
        if (!in) {
            throw std::runtime_error("unable to read chunk payload");
        }
        return deserialize_chunk(payload);
    }

    std::uint32_t flags = 0;
    in.read(reinterpret_cast<char*>(&flags), sizeof(flags));
    if (!in) {
        throw std::runtime_error("unable to read chunk channel flags");
    }

    chunk_header_v2 header_v2{};
    std::memcpy(&header_v2, &header_v1, sizeof(header_v1));
    header_v2.version = header_v1.version;
    header_v2.channel_flags = flags;

    const chunk_extent extent{header_v2.extent[0], header_v2.extent[1], header_v2.extent[2]};
    const auto count = extent.volume();
    std::size_t payload_bytes = count * (sizeof(voxel_id) + 3);
    if (flags & chunk_channel_materials) {
        payload_bytes += count * sizeof(material_index);
    }
    if (flags & chunk_channel_skylight_cache) {
        payload_bytes += count * sizeof(float);
    }
    if (flags & chunk_channel_blocklight_cache) {
        payload_bytes += count * sizeof(float);
    }

    std::vector<std::byte> payload(sizeof(chunk_header_v2) + payload_bytes);
    std::memcpy(payload.data(), &header_v2, sizeof(header_v2));
    in.read(reinterpret_cast<char*>(payload.data() + sizeof(header_v2)), static_cast<std::streamsize>(payload_bytes));
    if (!in) {
        throw std::runtime_error("unable to read chunk payload");
    }
    return deserialize_chunk(payload);
}

inline region_blob serialize_snapshot(const region_manager::region_snapshot& snapshot) {
    region_blob blob;
    blob.key = snapshot.key;
    if (snapshot.chunk) {
        blob.payload = serialize_chunk(*snapshot.chunk);
    }
    return blob;
}

template <typename Sink>
auto make_region_serializer(Sink&& sink) {
    return [sink = std::forward<Sink>(sink)](const region_manager::region_snapshot& snapshot) mutable {
        sink(serialize_snapshot(snapshot));
    };
}

template <typename Sink>
void dump_region(const region_manager& manager, Sink&& sink, bool include_clean = false) {
    auto&& callable = std::forward<Sink>(sink);
    for (const auto& snapshot : manager.snapshot_loaded(include_clean)) {
        callable(snapshot);
    }
}

inline auto file_sink(const std::filesystem::path& path) {
    return [path](const region_blob& blob) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) {
            throw std::runtime_error("failed to open region file");
        }
        out.write(reinterpret_cast<const char*>(&blob.key), sizeof(blob.key));
        const std::uint32_t size = static_cast<std::uint32_t>(blob.payload.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        out.write(reinterpret_cast<const char*>(blob.payload.data()), static_cast<std::streamsize>(blob.payload.size()));
    };
}

inline std::optional<region_blob> read_region_blob(std::istream& in) {
    region_blob blob;
    in.read(reinterpret_cast<char*>(&blob.key), sizeof(blob.key));
    if (!in) {
        return std::nullopt;
    }
    std::uint32_t size = 0;
    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!in) {
        return std::nullopt;
    }
    blob.payload.resize(size);
    in.read(reinterpret_cast<char*>(blob.payload.data()), static_cast<std::streamsize>(size));
    if (!in) {
        return std::nullopt;
    }
    return blob;
}

inline void ingest_blob(region_manager& manager, const region_blob& blob) {
    chunk_storage chunk = deserialize_chunk(blob.payload);
    auto& target = manager.assure(blob.key);
    target = std::move(chunk);
    target.mark_dirty(false);
}

} // namespace almond::voxel::serialization
