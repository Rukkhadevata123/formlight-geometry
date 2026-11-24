export module occt_lab.geometry.surfaces.patch_grid;

import std;

namespace occt_lab::geometry
{
export struct PatchGrid
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

std::vector<std::uint32_t> PatchGrid::triangle_indices() const
{
    if (!valid())
    {
        throw std::out_of_range("Surface grid subdivisions outside supported range");
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(index_count());
    const auto stride = u_segments + 1;
    for (std::uint32_t v = 0; v < v_segments; ++v)
    {
        for (std::uint32_t u = 0; u < u_segments; ++u)
        {
            const auto a = v * stride + u;
            indices.insert(indices.end(), {a, a + 1, a + stride + 1, a, a + stride + 1, a + stride});
        }
    }
    return indices;
}
} // namespace occt_lab::geometry
