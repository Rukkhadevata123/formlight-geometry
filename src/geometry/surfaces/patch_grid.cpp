#include "patch_grid.hpp"

#include <stdexcept>

namespace occt_lab::geometry
{
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
