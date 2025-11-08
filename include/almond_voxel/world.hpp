#pragma once

#include "almond_voxel/chunk.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace almond::voxel {

struct region_key {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};

    [[nodiscard]] friend constexpr bool operator==(const region_key& lhs, const region_key& rhs) noexcept = default;
};

struct region_key_hash {
    [[nodiscard]] std::size_t operator()(const region_key& key) const noexcept {
        std::uint64_t hx = static_cast<std::uint64_t>(key.x);
        std::uint64_t hy = static_cast<std::uint64_t>(key.y);
        std::uint64_t hz = static_cast<std::uint64_t>(key.z);
        std::uint64_t hash = hx * 0x9E3779B185EBCA87ull;
        hash ^= hy + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
        hash ^= hz + 0xC2B2AE3D27D4EB4Full + (hash << 6) + (hash >> 2);
        return static_cast<std::size_t>(hash);
    }
};

class region_manager {
public:
    using chunk_ptr = std::shared_ptr<chunk_storage>;
    using loader_type = std::function<chunk_storage(const region_key&)>;
    using saver_type = std::function<void(const region_key&, const chunk_storage&)>;
    using task_type = std::function<void(chunk_storage&, const region_key&)>;

    explicit region_manager(chunk_extent chunk_dimensions = cubic_extent(32));

    [[nodiscard]] chunk_extent chunk_dimensions() const noexcept { return chunk_extent_; }

    chunk_storage& assure(const region_key& key);
    [[nodiscard]] chunk_ptr find(const region_key& key) const;

    void set_loader(loader_type loader) { loader_ = std::move(loader); }
    void set_saver(saver_type saver) { saver_ = std::move(saver); }

    void set_max_resident(std::size_t limit) noexcept;
    [[nodiscard]] std::size_t max_resident() const noexcept { return max_resident_; }
    [[nodiscard]] std::size_t resident() const noexcept { return regions_.size(); }

    void pin(const region_key& key);
    void unpin(const region_key& key);

    void enqueue_task(const region_key& key, task_type task);
    std::size_t tick(std::size_t budget = std::numeric_limits<std::size_t>::max());

    void for_each_loaded(const std::function<void(const region_key&, const chunk_storage&)>& visitor) const;

    struct region_snapshot {
        region_key key{};
        std::shared_ptr<const chunk_storage> chunk;
    };

    [[nodiscard]] std::vector<region_snapshot> snapshot_loaded(bool include_clean = false) const;

    bool unload(const region_key& key);
    void evict_until_within_limit();

private:
    struct entry {
        chunk_ptr chunk;
        bool pinned{false};
    };

    chunk_storage& load_or_create(const region_key& key);
    void touch(const region_key& key);

    chunk_extent chunk_extent_{};
    std::unordered_map<region_key, entry, region_key_hash> regions_{};
    std::deque<region_key> lru_{};
    std::size_t max_resident_{128};
    loader_type loader_{};
    saver_type saver_{};
    std::deque<std::pair<region_key, task_type>> task_queue_{};
};

inline region_manager::region_manager(chunk_extent chunk_dimensions)
    : chunk_extent_{chunk_dimensions} {
}

inline chunk_storage& region_manager::assure(const region_key& key) {
    auto& chunk = load_or_create(key);
    touch(key);
    return chunk;
}

inline region_manager::chunk_ptr region_manager::find(const region_key& key) const {
    if (auto it = regions_.find(key); it != regions_.end()) {
        return it->second.chunk;
    }
    return {};
}

inline void region_manager::set_max_resident(std::size_t limit) noexcept {
    max_resident_ = limit;
    evict_until_within_limit();
}

inline void region_manager::pin(const region_key& key) {
    if (auto it = regions_.find(key); it != regions_.end()) {
        it->second.pinned = true;
    }
}

inline void region_manager::unpin(const region_key& key) {
    if (auto it = regions_.find(key); it != regions_.end()) {
        it->second.pinned = false;
    }
}

inline void region_manager::enqueue_task(const region_key& key, task_type task) {
    task_queue_.emplace_back(key, std::move(task));
}

inline std::size_t region_manager::tick(std::size_t budget) {
    std::size_t processed = 0;
    while (processed < budget && !task_queue_.empty()) {
        auto [key, task] = std::move(task_queue_.front());
        task_queue_.pop_front();
        auto& chunk = assure(key);
        if (task) {
            task(chunk, key);
        }
        ++processed;
    }
    evict_until_within_limit();
    return processed;
}

inline void region_manager::for_each_loaded(const std::function<void(const region_key&, const chunk_storage&)>& visitor) const {
    for (const auto& [key, entry] : regions_) {
        if (entry.chunk) {
            visitor(key, *entry.chunk);
        }
    }
}

inline std::vector<region_manager::region_snapshot> region_manager::snapshot_loaded(bool include_clean) const {
    std::vector<region_snapshot> snapshots;
    snapshots.reserve(regions_.size());
    for (const auto& [key, entry] : regions_) {
        if (!entry.chunk) {
            continue;
        }
        if (!include_clean && !entry.chunk->dirty()) {
            continue;
        }
        snapshots.push_back(region_snapshot{key, std::const_pointer_cast<const chunk_storage>(entry.chunk)});
    }
    return snapshots;
}

inline bool region_manager::unload(const region_key& key) {
    auto it = regions_.find(key);
    if (it == regions_.end()) {
        return false;
    }
    if (it->second.pinned) {
        return false;
    }
    if (it->second.chunk && saver_ && it->second.chunk->dirty()) {
        saver_(key, *it->second.chunk);
    }
    regions_.erase(it);
    return true;
}

inline void region_manager::evict_until_within_limit() {
    while (regions_.size() > max_resident_ && !lru_.empty()) {
        const auto key = lru_.front();
        lru_.pop_front();
        auto it = regions_.find(key);
        if (it == regions_.end() || it->second.pinned) {
            continue;
        }
        if (it->second.chunk && saver_ && it->second.chunk->dirty()) {
            saver_(key, *it->second.chunk);
        }
        regions_.erase(it);
    }
}

inline chunk_storage& region_manager::load_or_create(const region_key& key) {
    if (auto it = regions_.find(key); it != regions_.end()) {
        return *it->second.chunk;
    }
    chunk_ptr chunk;
    if (loader_) {
        chunk = std::make_shared<chunk_storage>(loader_(key));
    } else {
        chunk = std::make_shared<chunk_storage>(chunk_extent_);
    }
    auto [it, inserted] = regions_.emplace(key, entry{std::move(chunk), false});
    (void)inserted;
    return *it->second.chunk;
}

inline void region_manager::touch(const region_key& key) {
    std::erase(lru_, key);
    lru_.push_back(key);
}

} // namespace almond::voxel
