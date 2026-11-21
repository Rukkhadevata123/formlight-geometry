#pragma once

#include "geometry/surfaces/bspline_surface.hpp"

namespace occt_lab
{
struct PlanarSurfaceParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    int u_poles = 4;
    int v_poles = 4;
    double width_mm = 90;
    double height_mm = 72;
};

[[nodiscard]] geometry::BSplineSurfaceDefinition make_saddle_surface();
[[nodiscard]] auto make_planar_nurbs_surface(const PlanarSurfaceParameters &parameters)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>;
} // namespace occt_lab
