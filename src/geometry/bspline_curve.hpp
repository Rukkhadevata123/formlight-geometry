#pragma once

#include "vec3.hpp"

#include <span>

namespace occt_lab::geometry
{
// Samples a non-periodic cubic B-spline with uniformly spaced interior knots, including both endpoints.
void sample_clamped_cubic_bspline(std::span<const Vec3> control_points, std::span<Vec3> samples);
} // namespace occt_lab::geometry
