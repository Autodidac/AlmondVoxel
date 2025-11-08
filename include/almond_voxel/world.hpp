#pragma once

#include "almond_voxel/chunk.hpp"
#include "almond_voxel/navigation/voxel_nav.hpp"
#include "almond_voxel/world_fwd.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace almond::voxel {

class region_manager {
public:
    using chunk_ptr = std::shared_ptr<chunk_storage>;
    using nav_grid_ptr = std::shared_ptr<navigation::nav_grid>;
    using loader_type = std::function<chunk_storage(const region_key&)>;
    using saver_type = std::function<void(const region_key&, const chunk_storage&)>;
    using task_type = std::function<void(chunk_storage&, const region_key&)>;
    using dirty_observer = std::function<void(const region_key&)>;

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

    void add_dirty_observer(dirty_observer observer);

    void enable_navigation(bool enable = true);
    void set_navigation_build_config(navigation::nav_build_config config);
    [[nodiscard]] std::shared_ptr<const navigation::nav_grid> navigation_grid(const region_key& key) const;
    void request_navigation_rebuild(const region_key& key);
    [[nodiscard]] navigation::stitched_nav_graph stitch_navigation(const region_key& origin,
        std::span<const region_key> neighbors) const;

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

    struct nav_cache_entry {
        nav_grid_ptr grid;
        bool dirty{true};
        bool rebuild_pending{false};
        std::size_t revision{0};
    };

    chunk_storage& load_or_create(const region_key& key);
    void touch(const region_key& key);
    void mark_nav_dirty(const region_key& key);
    void schedule_nav_rebuild(const region_key& key);
    void clear_nav_cache(const region_key& key);

    chunk_extent chunk_extent_{};
    std::unordered_map<region_key, entry, region_key_hash> regions_{};
    std::deque<region_key> lru_{};
    std::size_t max_resident_{128};
    loader_type loader_{};
    saver_type saver_{};
    std::deque<std::pair<region_key, task_type>> task_queue_{};
    std::vector<dirty_observer> dirty_observers_{};
    navigation::nav_build_config nav_config_{};
    bool navigation_enabled_{false};
    std::unordered_map<region_key, nav_cache_entry, region_key_hash> nav_cache_{};
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
        touch(key);
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

inline void region_manager::add_dirty_observer(dirty_observer observer) {
    dirty_observers_.push_back(std::move(observer));
}

inline void region_manager::enable_navigation(bool enable) {
    if (navigation_enabled_ == enable) {
        return;
    }
    navigation_enabled_ = enable;
    if (!navigation_enabled_) {
        nav_cache_.clear();
        return;
    }
    nav_cache_.clear();
    for (const auto& [key, entry] : regions_) {
        if (entry.chunk) {
            mark_nav_dirty(key);
        }
    }
}

inline void region_manager::set_navigation_build_config(navigation::nav_build_config config) {
    nav_config_ = std::move(config);
    if (!navigation_enabled_) {
        return;
    }
    for (const auto& [key, entry] : regions_) {
        if (entry.chunk) {
            mark_nav_dirty(key);
        }
    }
}

inline std::shared_ptr<const navigation::nav_grid> region_manager::navigation_grid(const region_key& key) const {
    if (!navigation_enabled_) {
        return {};
    }
    if (auto it = nav_cache_.find(key); it != nav_cache_.end()) {
        return it->second.grid;
    }
    return {};
}

inline void region_manager::request_navigation_rebuild(const region_key& key) {
    if (!navigation_enabled_) {
        return;
    }
    mark_nav_dirty(key);
}

inline navigation::stitched_nav_graph region_manager::stitch_navigation(const region_key& origin,
    std::span<const region_key> neighbors) const {
    navigation::stitched_nav_graph stitched;
    if (!navigation_enabled_) {
        return stitched;
    }

    const auto add_region = [&](const region_key& key) {
        if (auto it = nav_cache_.find(key); it != nav_cache_.end()) {
            if (it->second.grid) {
                stitched.regions.push_back(navigation::nav_region_view{key, it->second.grid});
            }
        }
    };

    add_region(origin);
    for (const auto& neighbor : neighbors) {
        add_region(neighbor);
    }

    navigation::stitch_neighbor_regions(nav_config_.neighbor, chunk_extent_, stitched);
    return stitched;
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
    clear_nav_cache(key);
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
        clear_nav_cache(key);
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
    if (chunk) {
        chunk->add_dirty_listener([this, key]() {
            mark_nav_dirty(key);
            for (auto& observer : dirty_observers_) {
                if (observer) {
                    observer(key);
                }
            }
        });
    }
    auto [it, inserted] = regions_.emplace(key, entry{std::move(chunk), false});
    (void)inserted;
    if (navigation_enabled_) {
        mark_nav_dirty(key);
    }
    return *it->second.chunk;
}

inline void region_manager::touch(const region_key& key) {
    std::erase(lru_, key);
    lru_.push_back(key);
}

inline void region_manager::mark_nav_dirty(const region_key& key) {
    if (!navigation_enabled_) {
        return;
    }
    auto& entry = nav_cache_[key];
    entry.dirty = true;
    schedule_nav_rebuild(key);
}

inline void region_manager::schedule_nav_rebuild(const region_key& key) {
    if (!navigation_enabled_) {
        return;
    }
    auto& entry = nav_cache_[key];
    if (entry.rebuild_pending) {
        return;
    }
    entry.rebuild_pending = true;
    task_queue_.emplace_back(key, [this, key](chunk_storage& chunk, const region_key&) {
        auto grid = std::make_shared<navigation::nav_grid>(
            navigation::build_nav_grid(static_cast<const chunk_storage&>(chunk), nav_config_));
        if (auto it = nav_cache_.find(key); it != nav_cache_.end()) {
            it->second.grid = std::move(grid);
            it->second.dirty = false;
            it->second.rebuild_pending = false;
            ++it->second.revision;
        }
    });
}

inline void region_manager::clear_nav_cache(const region_key& key) {
    nav_cache_.erase(key);
}

} // namespace almond::voxel
