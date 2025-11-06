#pragma once

#include "almond_voxel/core.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace almond::voxel {

class chunk_storage {
public:
    using byte_vector = std::vector<std::byte>;
    using compress_callback = std::function<byte_vector(voxel_cspan<voxel_id>)>;
    using decompress_callback = std::function<void(voxel_span<voxel_id>, std::span<const std::byte>)>;

    explicit chunk_storage(chunk_extent extent = cubic_extent(32));
    chunk_storage(const chunk_storage&) = delete;
    chunk_storage& operator=(const chunk_storage&) = delete;
    chunk_storage(chunk_storage&& other) noexcept;
    chunk_storage& operator=(chunk_storage&& other) noexcept;

    [[nodiscard]] chunk_extent extent() const noexcept { return extent_; }
    [[nodiscard]] std::size_t volume() const noexcept { return extent_.volume(); }

    [[nodiscard]] span3d<voxel_id> voxels() noexcept;
    [[nodiscard]] span3d<const voxel_id> voxels() const noexcept;

    [[nodiscard]] span3d<std::uint8_t> skylight() noexcept;
    [[nodiscard]] span3d<const std::uint8_t> skylight() const noexcept;

    [[nodiscard]] span3d<std::uint8_t> blocklight() noexcept;
    [[nodiscard]] span3d<const std::uint8_t> blocklight() const noexcept;

    [[nodiscard]] span3d<std::uint8_t> metadata() noexcept;
    [[nodiscard]] span3d<const std::uint8_t> metadata() const noexcept;

    void fill(voxel_id voxel, std::uint8_t sky_level = 0, std::uint8_t block_level = 0, std::uint8_t meta = 0);
    void assign_voxels(voxel_cspan<voxel_id> data);

    void set_compression_hooks(compress_callback compressor, decompress_callback decompressor = {});
    void request_compression() noexcept { compression_requested_ = true; }
    [[nodiscard]] bool flush_compression();
    [[nodiscard]] bool decompress();

    [[nodiscard]] bool compressed() const noexcept { return compressed_; }
    [[nodiscard]] std::span<const std::byte> compressed_blob() const noexcept { return compressed_blob_; }

    void clear_compression() {
        compression_requested_ = false;
        compressed_ = false;
        compressed_blob_.clear();
    }

    void mark_dirty(bool value = true) noexcept { dirty_ = value; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

private:
    void ensure_capacity();
    void ensure_decompressed();
    void decompress_locked();

    chunk_extent extent_{};
    std::vector<voxel_id> voxels_{};
    std::vector<std::uint8_t> skylight_{};
    std::vector<std::uint8_t> blocklight_{};
    std::vector<std::uint8_t> metadata_{};

    compress_callback compress_{};
    decompress_callback decompress_{};

    bool dirty_{false};
    bool compression_requested_{false};
    bool compressed_{false};
    byte_vector compressed_blob_{};
    std::mutex compression_mutex_{};
};

inline chunk_storage::chunk_storage(chunk_extent extent)
    : extent_{extent} {
    ensure_capacity();
}

inline chunk_storage::chunk_storage(chunk_storage&& other) noexcept
    : extent_{other.extent_}
    , voxels_{std::move(other.voxels_)}
    , skylight_{std::move(other.skylight_)}
    , blocklight_{std::move(other.blocklight_)}
    , metadata_{std::move(other.metadata_)}
    , compress_{std::move(other.compress_)}
    , decompress_{std::move(other.decompress_)}
    , dirty_{other.dirty_}
    , compression_requested_{other.compression_requested_}
    , compressed_{other.compressed_}
    , compressed_blob_{std::move(other.compressed_blob_)} {
    other.extent_ = chunk_extent{};
    other.dirty_ = false;
    other.compression_requested_ = false;
    other.compressed_ = false;
}

inline chunk_storage& chunk_storage::operator=(chunk_storage&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock{compression_mutex_, other.compression_mutex_};
        extent_ = other.extent_;
        voxels_ = std::move(other.voxels_);
        skylight_ = std::move(other.skylight_);
        blocklight_ = std::move(other.blocklight_);
        metadata_ = std::move(other.metadata_);
        compress_ = std::move(other.compress_);
        decompress_ = std::move(other.decompress_);
        dirty_ = other.dirty_;
        compression_requested_ = other.compression_requested_;
        compressed_ = other.compressed_;
        compressed_blob_ = std::move(other.compressed_blob_);

        other.extent_ = chunk_extent{};
        other.dirty_ = false;
        other.compression_requested_ = false;
        other.compressed_ = false;
        other.compressed_blob_.clear();
        other.compress_ = {};
        other.decompress_ = {};
    }
    return *this;
}

inline span3d<voxel_id> chunk_storage::voxels() noexcept {
    ensure_decompressed();
    mark_dirty();
    return make_span3d(voxels_.data(), extent_);
}

inline span3d<const voxel_id> chunk_storage::voxels() const noexcept {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    return make_span3d(voxels_.data(), extent_);
}

inline span3d<std::uint8_t> chunk_storage::skylight() noexcept {
    ensure_decompressed();
    mark_dirty();
    return make_span3d(skylight_.data(), extent_);
}

inline span3d<const std::uint8_t> chunk_storage::skylight() const noexcept {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    return make_span3d(skylight_.data(), extent_);
}

inline span3d<std::uint8_t> chunk_storage::blocklight() noexcept {
    ensure_decompressed();
    mark_dirty();
    return make_span3d(blocklight_.data(), extent_);
}

inline span3d<const std::uint8_t> chunk_storage::blocklight() const noexcept {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    return make_span3d(blocklight_.data(), extent_);
}

inline span3d<std::uint8_t> chunk_storage::metadata() noexcept {
    ensure_decompressed();
    mark_dirty();
    return make_span3d(metadata_.data(), extent_);
}

inline span3d<const std::uint8_t> chunk_storage::metadata() const noexcept {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    return make_span3d(metadata_.data(), extent_);
}

inline void chunk_storage::fill(voxel_id voxel, std::uint8_t sky_level, std::uint8_t block_level, std::uint8_t meta) {
    ensure_decompressed();
    std::fill(voxels_.begin(), voxels_.end(), voxel);
    std::fill(skylight_.begin(), skylight_.end(), sky_level);
    std::fill(blocklight_.begin(), blocklight_.end(), block_level);
    std::fill(metadata_.begin(), metadata_.end(), meta);
    mark_dirty();
}

inline void chunk_storage::assign_voxels(voxel_cspan<voxel_id> data) {
    ensure_decompressed();
    if (data.size() != voxels_.size()) {
        throw std::runtime_error("voxel data size mismatch");
    }
    std::copy(data.begin(), data.end(), voxels_.begin());
    mark_dirty();
}

inline void chunk_storage::set_compression_hooks(compress_callback compressor, decompress_callback decompressor) {
    std::scoped_lock lock{compression_mutex_};
    compress_ = std::move(compressor);
    decompress_ = std::move(decompressor);
}

inline bool chunk_storage::flush_compression() {
    std::scoped_lock lock{compression_mutex_};
    if (!compression_requested_ || !compress_) {
        return false;
    }
    decompress_locked();
    voxel_cspan<voxel_id> view = make_span3d(voxels_.data(), extent_).linear();
    compressed_blob_ = compress_(view);
    compression_requested_ = false;
    compressed_ = true;
    return true;
}

inline bool chunk_storage::decompress() {
    std::scoped_lock lock{compression_mutex_};
    if (!compressed_ || compressed_blob_.empty()) {
        return false;
    }
    decompress_locked();
    return true;
}

inline void chunk_storage::ensure_capacity() {
    const auto count = extent_.volume();
    voxels_.resize(count, voxel_id{});
    skylight_.resize(count, std::uint8_t{});
    blocklight_.resize(count, std::uint8_t{});
    metadata_.resize(count, std::uint8_t{});
}

inline void chunk_storage::ensure_decompressed() {
    if (compressed_) {
        std::scoped_lock lock{compression_mutex_};
        decompress_locked();
    }
}

inline void chunk_storage::decompress_locked() {
    if (!compressed_ || compressed_blob_.empty()) {
        return;
    }
    if (decompress_) {
        decompress_(make_span3d(voxels_.data(), extent_).linear(), compressed_blob_);
    }
    compressed_blob_.clear();
    compressed_ = false;
}

} // namespace almond::voxel
