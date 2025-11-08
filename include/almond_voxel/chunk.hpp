#pragma once

#include "almond_voxel/core.hpp"
#include "almond_voxel/material/voxel_material.hpp"

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

struct chunk_storage_config {
    chunk_extent extent{cubic_extent(32)};
    bool enable_materials{false};
    bool enable_high_precision_lighting{false};
};

class chunk_storage {
public:
    using byte_vector = std::vector<std::byte>;
    struct planes_view {
        voxel_span<voxel_id> voxels{};
        voxel_span<std::uint8_t> skylight{};
        voxel_span<std::uint8_t> blocklight{};
        voxel_span<std::uint8_t> metadata{};
        voxel_span<material_index> materials{};
        std::span<float> skylight_cache{};
        std::span<float> blocklight_cache{};
    };
    struct const_planes_view {
        voxel_cspan<voxel_id> voxels{};
        voxel_cspan<std::uint8_t> skylight{};
        voxel_cspan<std::uint8_t> blocklight{};
        voxel_cspan<std::uint8_t> metadata{};
        std::span<const material_index> materials{};
        std::span<const float> skylight_cache{};
        std::span<const float> blocklight_cache{};
    };
    using compress_callback = std::function<byte_vector(const const_planes_view&)>;
    using decompress_callback = std::function<void(const planes_view&, std::span<const std::byte>)>;
    using dirty_listener = std::function<void()>;

    explicit chunk_storage(chunk_extent extent = cubic_extent(32));
    explicit chunk_storage(chunk_storage_config config);
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

    [[nodiscard]] bool materials_enabled() const noexcept { return materials_enabled_; }
    [[nodiscard]] span3d<material_index> materials();
    [[nodiscard]] span3d<const material_index> materials() const;

    [[nodiscard]] bool high_precision_lighting_enabled() const noexcept { return high_precision_lighting_enabled_; }
    [[nodiscard]] span3d<float> skylight_cache();
    [[nodiscard]] span3d<const float> skylight_cache() const;
    [[nodiscard]] span3d<float> blocklight_cache();
    [[nodiscard]] span3d<const float> blocklight_cache() const;

    void fill(voxel_id voxel, std::uint8_t sky_level = 0, std::uint8_t block_level = 0, std::uint8_t meta = 0,
        material_index material = invalid_material_index, float sky_cache = 0.0f, float block_cache = 0.0f);
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

    void mark_dirty(bool value = true) noexcept;
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

    void add_dirty_listener(dirty_listener listener);
    void clear_dirty_listeners();

private:
    void ensure_capacity();
    [[nodiscard]] planes_view make_planes_view() noexcept;
    [[nodiscard]] const_planes_view make_const_planes_view() const noexcept;
    void ensure_decompressed();
    void decompress_locked();

    chunk_extent extent_{};
    std::vector<voxel_id> voxels_{};
    std::vector<std::uint8_t> skylight_{};
    std::vector<std::uint8_t> blocklight_{};
    std::vector<std::uint8_t> metadata_{};
    bool materials_enabled_{false};
    bool high_precision_lighting_enabled_{false};
    std::vector<material_index> materials_{};
    std::vector<float> skylight_cache_{};
    std::vector<float> blocklight_cache_{};

    compress_callback compress_{};
    decompress_callback decompress_{};

    bool dirty_{false};
    bool compression_requested_{false};
    bool compressed_{false};
    byte_vector compressed_blob_{};
    std::mutex compression_mutex_{};
    std::vector<dirty_listener> dirty_listeners_{};
};

inline chunk_storage::chunk_storage(chunk_extent extent)
    : chunk_storage(chunk_storage_config{extent}) {
}

inline chunk_storage::chunk_storage(chunk_storage_config config)
    : extent_{config.extent}
    , materials_enabled_{config.enable_materials}
    , high_precision_lighting_enabled_{config.enable_high_precision_lighting} {
    ensure_capacity();
}

inline chunk_storage::chunk_storage(chunk_storage&& other) noexcept
    : extent_{other.extent_}
    , voxels_{std::move(other.voxels_)}
    , skylight_{std::move(other.skylight_)}
    , blocklight_{std::move(other.blocklight_)}
    , metadata_{std::move(other.metadata_)}
    , materials_enabled_{other.materials_enabled_}
    , high_precision_lighting_enabled_{other.high_precision_lighting_enabled_}
    , materials_{std::move(other.materials_)}
    , skylight_cache_{std::move(other.skylight_cache_)}
    , blocklight_cache_{std::move(other.blocklight_cache_)}
    , compress_{std::move(other.compress_)}
    , decompress_{std::move(other.decompress_)}
    , dirty_{other.dirty_}
    , compression_requested_{other.compression_requested_}
    , compressed_{other.compressed_}
    , compressed_blob_{std::move(other.compressed_blob_)}
    , dirty_listeners_{std::move(other.dirty_listeners_)} {
    other.extent_ = chunk_extent{};
    other.materials_enabled_ = false;
    other.high_precision_lighting_enabled_ = false;
    other.materials_.clear();
    other.skylight_cache_.clear();
    other.blocklight_cache_.clear();
    other.dirty_ = false;
    other.compression_requested_ = false;
    other.compressed_ = false;
    other.dirty_listeners_.clear();
}

inline chunk_storage& chunk_storage::operator=(chunk_storage&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock{compression_mutex_, other.compression_mutex_};
        extent_ = other.extent_;
        voxels_ = std::move(other.voxels_);
        skylight_ = std::move(other.skylight_);
        blocklight_ = std::move(other.blocklight_);
        metadata_ = std::move(other.metadata_);
        materials_enabled_ = other.materials_enabled_;
        high_precision_lighting_enabled_ = other.high_precision_lighting_enabled_;
        materials_ = std::move(other.materials_);
        skylight_cache_ = std::move(other.skylight_cache_);
        blocklight_cache_ = std::move(other.blocklight_cache_);
        compress_ = std::move(other.compress_);
        decompress_ = std::move(other.decompress_);
        dirty_ = other.dirty_;
        compression_requested_ = other.compression_requested_;
        compressed_ = other.compressed_;
        compressed_blob_ = std::move(other.compressed_blob_);
        dirty_listeners_ = std::move(other.dirty_listeners_);

        other.extent_ = chunk_extent{};
        other.materials_enabled_ = false;
        other.high_precision_lighting_enabled_ = false;
        other.materials_.clear();
        other.skylight_cache_.clear();
        other.blocklight_cache_.clear();
        other.dirty_ = false;
        other.compression_requested_ = false;
        other.compressed_ = false;
        other.compressed_blob_.clear();
        other.compress_ = {};
        other.decompress_ = {};
        other.dirty_listeners_.clear();
    }
    return *this;
}

inline void chunk_storage::mark_dirty(bool value) noexcept {
    const bool notify = value;
    if (dirty_ != value) {
        dirty_ = value;
    }
    if (notify && !dirty_listeners_.empty()) {
        for (auto& listener : dirty_listeners_) {
            if (listener) {
                listener();
            }
        }
    }
}

inline void chunk_storage::add_dirty_listener(dirty_listener listener) {
    dirty_listeners_.push_back(std::move(listener));
}

inline void chunk_storage::clear_dirty_listeners() {
    dirty_listeners_.clear();
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

inline span3d<material_index> chunk_storage::materials() {
    ensure_decompressed();
    if (!materials_enabled_) {
        throw std::logic_error("material plane is disabled");
    }
    mark_dirty();
    return make_span3d(materials_.data(), extent_);
}

inline span3d<const material_index> chunk_storage::materials() const {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    if (!materials_enabled_) {
        throw std::logic_error("material plane is disabled");
    }
    return make_span3d(materials_.data(), extent_);
}

inline span3d<float> chunk_storage::skylight_cache() {
    ensure_decompressed();
    if (!high_precision_lighting_enabled_) {
        throw std::logic_error("high precision lighting cache is disabled");
    }
    mark_dirty();
    return make_span3d(skylight_cache_.data(), extent_);
}

inline span3d<const float> chunk_storage::skylight_cache() const {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    if (!high_precision_lighting_enabled_) {
        throw std::logic_error("high precision lighting cache is disabled");
    }
    return make_span3d(skylight_cache_.data(), extent_);
}

inline span3d<float> chunk_storage::blocklight_cache() {
    ensure_decompressed();
    if (!high_precision_lighting_enabled_) {
        throw std::logic_error("high precision lighting cache is disabled");
    }
    mark_dirty();
    return make_span3d(blocklight_cache_.data(), extent_);
}

inline span3d<const float> chunk_storage::blocklight_cache() const {
    const_cast<chunk_storage*>(this)->ensure_decompressed();
    if (!high_precision_lighting_enabled_) {
        throw std::logic_error("high precision lighting cache is disabled");
    }
    return make_span3d(blocklight_cache_.data(), extent_);
}

inline void chunk_storage::fill(voxel_id voxel, std::uint8_t sky_level, std::uint8_t block_level, std::uint8_t meta,
    material_index material, float sky_cache, float block_cache) {
    ensure_decompressed();
    std::fill(voxels_.begin(), voxels_.end(), voxel);
    std::fill(skylight_.begin(), skylight_.end(), sky_level);
    std::fill(blocklight_.begin(), blocklight_.end(), block_level);
    std::fill(metadata_.begin(), metadata_.end(), meta);
    if (materials_enabled_) {
        std::fill(materials_.begin(), materials_.end(), material);
    }
    if (high_precision_lighting_enabled_) {
        std::fill(skylight_cache_.begin(), skylight_cache_.end(), sky_cache);
        std::fill(blocklight_cache_.begin(), blocklight_cache_.end(), block_cache);
    }
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
    const auto view = make_const_planes_view();
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
    voxels_.assign(count, voxel_id{});
    skylight_.assign(count, std::uint8_t{});
    blocklight_.assign(count, std::uint8_t{});
    metadata_.assign(count, std::uint8_t{});
    if (materials_enabled_) {
        materials_.assign(count, invalid_material_index);
    } else {
        materials_.clear();
    }
    if (high_precision_lighting_enabled_) {
        skylight_cache_.assign(count, 0.0f);
        blocklight_cache_.assign(count, 0.0f);
    } else {
        skylight_cache_.clear();
        blocklight_cache_.clear();
    }
}

inline chunk_storage::planes_view chunk_storage::make_planes_view() noexcept {
    planes_view view{};
    view.voxels = make_span3d(voxels_.data(), extent_).linear();
    view.skylight = make_span3d(skylight_.data(), extent_).linear();
    view.blocklight = make_span3d(blocklight_.data(), extent_).linear();
    view.metadata = make_span3d(metadata_.data(), extent_).linear();
    if (materials_enabled_) {
        view.materials = make_span3d(materials_.data(), extent_).linear();
    }
    if (high_precision_lighting_enabled_) {
        view.skylight_cache = make_span3d(skylight_cache_.data(), extent_).linear();
        view.blocklight_cache = make_span3d(blocklight_cache_.data(), extent_).linear();
    }
    return view;
}

inline chunk_storage::const_planes_view chunk_storage::make_const_planes_view() const noexcept {
    const_planes_view view{};
    view.voxels = make_span3d(voxels_.data(), extent_).linear();
    view.skylight = make_span3d(skylight_.data(), extent_).linear();
    view.blocklight = make_span3d(blocklight_.data(), extent_).linear();
    view.metadata = make_span3d(metadata_.data(), extent_).linear();
    if (materials_enabled_) {
        view.materials = make_span3d(materials_.data(), extent_).linear();
    }
    if (high_precision_lighting_enabled_) {
        view.skylight_cache = make_span3d(skylight_cache_.data(), extent_).linear();
        view.blocklight_cache = make_span3d(blocklight_cache_.data(), extent_).linear();
    }
    return view;
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
        decompress_(make_planes_view(), compressed_blob_);
    }
    compressed_blob_.clear();
    compressed_ = false;
}

} // namespace almond::voxel
