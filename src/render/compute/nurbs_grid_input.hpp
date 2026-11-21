#pragma once

#include "geometry/surfaces/bspline_surface.hpp"

#include <array>
#include <cstddef>

namespace occt_lab::render
{
struct NurbsGridVertex
{
    std::array<float, 4> position; // Local XYZ and rational denominator.
    std::array<float, 4> normal;   // Unit XYZ; W is 1 for valid, 0 for unavailable, -1 for numerical failure.
};
static_assert(sizeof(NurbsGridVertex) == 8 * sizeof(float) && offsetof(NurbsGridVertex, normal) == 4 * sizeof(float));

// Shader binding 0: grid vertices, pole float4s, then each axis's expanded knots
// followed by (parameter, packed span and crease flag) pairs.
struct NurbsGridInput
{
    std::array<std::uint32_t, 6> parameters; // U/V segments, U/V pole counts, U/V degrees.
    std::vector<float> words;
    geometry::Vec3 origin;
    double extent_mm;
};

// Accepts a definition already validated by the editor.
[[nodiscard]] NurbsGridInput pack_nurbs_grid(const geometry::BSplineSurfaceDefinition &definition,
                                             geometry::PatchGrid grid, std::uint32_t max_buffer_bytes);
} // namespace occt_lab::render
