#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace almond::voxel {

using voxel_id = std::uint16_t;

enum class axis : std::uint8_t { x = 0, y = 1, z = 2 };

enum class block_face : std::uint8_t {
    pos_x = 0,
    neg_x = 1,
    pos_y = 2,
    neg_y = 3,
    pos_z = 4,
    neg_z = 5
};

constexpr std::size_t block_face_count = 6;

[[nodiscard]] constexpr axis axis_of(block_face face) noexcept {
    switch (face) {
    case block_face::pos_x:
    case block_face::neg_x:
        return axis::x;
    case block_face::pos_y:
    case block_face::neg_y:
        return axis::y;
    case block_face::pos_z:
    case block_face::neg_z:
    default:
        return axis::z;
    }
}

[[nodiscard]] constexpr int axis_sign(block_face face) noexcept {
    switch (face) {
    case block_face::pos_x:
    case block_face::pos_y:
    case block_face::pos_z:
        return 1;
    default:
        return -1;
    }
}

[[nodiscard]] constexpr block_face opposite(block_face face) noexcept {
    switch (face) {
    case block_face::pos_x:
        return block_face::neg_x;
    case block_face::neg_x:
        return block_face::pos_x;
    case block_face::pos_y:
        return block_face::neg_y;
    case block_face::neg_y:
        return block_face::pos_y;
    case block_face::pos_z:
        return block_face::neg_z;
    case block_face::neg_z:
    default:
        return block_face::pos_z;
    }
}

[[nodiscard]] constexpr std::array<int, 3> face_normal(block_face face) noexcept {
    switch (face) {
    case block_face::pos_x:
        return {1, 0, 0};
    case block_face::neg_x:
        return {-1, 0, 0};
    case block_face::pos_y:
        return {0, 1, 0};
    case block_face::neg_y:
        return {0, -1, 0};
    case block_face::pos_z:
        return {0, 0, 1};
    case block_face::neg_z:
    default:
        return {0, 0, -1};
    }
}

[[nodiscard]] constexpr std::string_view face_name(block_face face) noexcept {
    switch (face) {
    case block_face::pos_x:
        return "+X";
    case block_face::neg_x:
        return "-X";
    case block_face::pos_y:
        return "+Y";
    case block_face::neg_y:
        return "-Y";
    case block_face::pos_z:
        return "+Z";
    case block_face::neg_z:
    default:
        return "-Z";
    }
}

struct chunk_extent {
    std::uint32_t x{1};
    std::uint32_t y{1};
    std::uint32_t z{1};

    [[nodiscard]] constexpr std::array<std::uint32_t, 3> to_array() const noexcept {
        return {x, y, z};
    }

    [[nodiscard]] constexpr std::size_t volume() const noexcept {
        return static_cast<std::size_t>(x) * static_cast<std::size_t>(y) * static_cast<std::size_t>(z);
    }

    [[nodiscard]] constexpr bool contains(std::uint32_t px, std::uint32_t py, std::uint32_t pz) const noexcept {
        return px < x && py < y && pz < z;
    }

    [[nodiscard]] constexpr bool operator==(const chunk_extent&) const noexcept = default;
};

[[nodiscard]] constexpr chunk_extent cubic_extent(std::uint32_t edge) noexcept {
    return chunk_extent{edge, edge, edge};
}

template <typename T>
using voxel_span = std::span<T>;

template <typename T>
using voxel_cspan = std::span<const T>;

template <typename Element>
class span3d {
public:
    using element_type = Element;
    using value_type = std::remove_cv_t<Element>;
    using pointer = Element*;
    using reference = Element&;
    using size_type = std::size_t;

    constexpr span3d() noexcept = default;

    constexpr span3d(pointer data, chunk_extent extent) noexcept
        : data_{data}
        , extent_{extent} {
    }

    template <typename OtherElement>
    constexpr span3d(const span3d<OtherElement>& other) noexcept
        requires std::is_convertible_v<typename span3d<OtherElement>::pointer, pointer>
        : data_{other.data()}
        , extent_{other.extent()} {
    }

    [[nodiscard]] constexpr chunk_extent extent() const noexcept { return extent_; }

    [[nodiscard]] constexpr pointer data() const noexcept { return data_; }

    [[nodiscard]] constexpr size_type size() const noexcept { return extent_.volume(); }

    [[nodiscard]] constexpr bool empty() const noexcept { return data_ == nullptr || size() == 0; }

    [[nodiscard]] constexpr reference operator()(size_type px, size_type py, size_type pz) const noexcept {
        return data_[index(px, py, pz)];
    }

    [[nodiscard]] constexpr bool contains(size_type px, size_type py, size_type pz) const noexcept {
        return extent_.contains(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), static_cast<std::uint32_t>(pz));
    }

    [[nodiscard]] constexpr voxel_span<Element> linear() const noexcept {
        return voxel_span<Element>{data_, size()};
    }

    [[nodiscard]] constexpr size_type index(size_type px, size_type py, size_type pz) const noexcept {
        return px + static_cast<size_type>(extent_.x) * (py + static_cast<size_type>(extent_.y) * pz);
    }

private:
    pointer data_{nullptr};
    chunk_extent extent_{};
};

template <typename Element>
[[nodiscard]] constexpr span3d<Element> make_span3d(Element* data, chunk_extent extent) noexcept {
    return span3d<Element>{data, extent};
}

template <typename Element>
[[nodiscard]] constexpr span3d<const Element> make_span3d(const Element* data, chunk_extent extent) noexcept {
    return span3d<const Element>{data, extent};
}

} // namespace almond::voxel
