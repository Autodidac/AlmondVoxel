#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/world.hpp"

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

struct chunk_header {
    char magic[4]{'A', 'V', 'C', 'K'};
    std::uint32_t version{1};
    std::uint32_t extent[3]{1, 1, 1};
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

    chunk_header header{};
    header.extent[0] = extent.x;
    header.extent[1] = extent.y;
    header.extent[2] = extent.z;

    std::vector<std::byte> buffer;
    buffer.reserve(sizeof(chunk_header) + extent.volume() * (sizeof(voxel_id) + 3));
    append_bytes(buffer, &header, sizeof(chunk_header));

    const auto copy_span = [&buffer](auto span) {
        using value_type = typename decltype(span)::value_type;
        append_bytes(buffer, span.data(), span.size() * sizeof(value_type));
    };

    copy_span(voxel_data.linear());
    copy_span(sky_data.linear());
    copy_span(block_data.linear());
    copy_span(meta_data.linear());

    return buffer;
}

inline chunk_storage deserialize_chunk(std::span<const std::byte> bytes) {
    if (bytes.size() < sizeof(chunk_header)) {
        throw std::runtime_error("chunk payload too small");
    }
    const auto* header = reinterpret_cast<const chunk_header*>(bytes.data());
    if (std::string_view(header->magic, 4) != std::string_view{"AVCK", 4}) {
        throw std::runtime_error("invalid chunk magic");
    }
    chunk_extent extent{header->extent[0], header->extent[1], header->extent[2]};
    const std::size_t expected = extent.volume();

    const std::size_t payload_size = sizeof(chunk_header) + expected * (sizeof(voxel_id) + 3);
    if (bytes.size() < payload_size) {
        throw std::runtime_error("chunk payload truncated");
    }

    chunk_storage chunk{extent};
    auto* ptr = bytes.data() + sizeof(chunk_header);

    auto copy_into = [&ptr, expected](auto view) {
        using value_type = typename decltype(view)::element_type;
        std::memcpy(view.linear().data(), ptr, expected * sizeof(value_type));
        ptr += expected * sizeof(value_type);
    };

    copy_into(chunk.voxels());
    copy_into(chunk.skylight());
    copy_into(chunk.blocklight());
    copy_into(chunk.metadata());
    chunk.mark_dirty(false);
    return chunk;
}

inline void serialize_chunk_to_stream(const chunk_storage& chunk, std::ostream& out) {
    auto payload = serialize_chunk(chunk);
    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
}

inline chunk_storage deserialize_chunk_from_stream(std::istream& in) {
    chunk_header header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in) {
        throw std::runtime_error("unable to read chunk header");
    }
    if (std::string_view(header.magic, 4) != std::string_view{"AVCK", 4}) {
        throw std::runtime_error("invalid chunk magic");
    }
    chunk_extent extent{header.extent[0], header.extent[1], header.extent[2]};
    const std::size_t expected = extent.volume();
    std::vector<std::byte> payload;
    payload.resize(expected * (sizeof(voxel_id) + 3));
    in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!in) {
        throw std::runtime_error("unable to read chunk payload");
    }
    std::vector<std::byte> full;
    full.reserve(sizeof(chunk_header) + payload.size());
    append_bytes(full, &header, sizeof(header));
    full.insert(full.end(), payload.begin(), payload.end());
    return deserialize_chunk(full);
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
    target.assign_voxels(chunk.voxels().linear());
    auto sky = target.skylight();
    auto block = target.blocklight();
    auto meta = target.metadata();
    std::memcpy(sky.linear().data(), chunk.skylight().linear().data(), sky.linear().size_bytes());
    std::memcpy(block.linear().data(), chunk.blocklight().linear().data(), block.linear().size_bytes());
    std::memcpy(meta.linear().data(), chunk.metadata().linear().data(), meta.linear().size_bytes());
    target.mark_dirty(false);
}

} // namespace almond::voxel::serialization
