#pragma once

#include <cstddef>

namespace occt_lab::geometry
{
struct BSplineSurfaceDefinition;
}

void draw_nurbs_pole_inspector(const occt_lab::geometry::BSplineSurfaceDefinition &surface, std::size_t selected_pole);
