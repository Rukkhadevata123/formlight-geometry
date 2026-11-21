#pragma once

#include "geometry/surfaces/bspline_surface.hpp"

#include <array>

namespace occt_lab::render
{
// Shader binding 0, in order: output float4s, pole float4s (local XYZ and weight),
// then each axis's expanded knots followed by its grid parameters.
struct NurbsPositionInput
{
    std::array<std::uint32_t, 6> parameters; // U/V segments, U/V pole counts, U/V degrees.
    std::vector<float> words;
    geometry::Vec3 origin;
    double extent_mm;
};

// Accepts a definition already validated by the editor.
[[nodiscard]] NurbsPositionInput pack_nurbs_positions(const geometry::BSplineSurfaceDefinition &definition,
                                                      geometry::PatchGrid grid, std::uint32_t max_buffer_bytes);
} // namespace occt_lab::render
