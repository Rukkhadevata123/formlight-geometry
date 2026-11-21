#pragma once

#include <cstdint>
#include <vector>

namespace occt_lab::geometry
{
struct PatchGrid
{
    static constexpr std::uint32_t max_segments = 32;
    std::uint32_t u_segments = 24;
    std::uint32_t v_segments = 16;
    bool operator==(const PatchGrid &) const = default;

    [[nodiscard]] constexpr bool valid() const
    {
        return u_segments >= 1 && u_segments <= max_segments && v_segments >= 1 && v_segments <= max_segments;
    }

    // Both endpoints of each axis are vertices. Call after checking valid().
    [[nodiscard]] constexpr std::uint32_t vertex_count() const
    {
        return (u_segments + 1) * (v_segments + 1);
    }

    [[nodiscard]] constexpr std::uint32_t index_count() const
    {
        return 6 * u_segments * v_segments;
    }

    [[nodiscard]] std::vector<std::uint32_t> triangle_indices() const;
};
} // namespace occt_lab::geometry
