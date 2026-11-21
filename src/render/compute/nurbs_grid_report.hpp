#pragma once

#include "nurbs_grid_input.hpp"

#include <span>

namespace occt_lab::render
{
[[nodiscard]] std::string compare_nurbs_grid(std::span<const NurbsGridVertex> vertices,
                                             std::span<const geometry::SurfaceProbe> reference, geometry::Vec3 origin,
                                             double position_tolerance_mm);
}
