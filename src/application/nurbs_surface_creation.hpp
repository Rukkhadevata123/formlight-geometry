#pragma once

#include "geometry/surfaces/bezier_patch.hpp"
#include "geometry/surfaces/bspline_surface.hpp"

#include <optional>

namespace occt_lab
{
struct PlanarSurfaceParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    static constexpr int preferred_degree = 3;
    [[nodiscard]] static int degree_limit(int pole_count);
    int u_poles = 4;
    int v_poles = 4;
    double width_mm = 90;
    double height_mm = 72;
    // An unset degree follows the default for that direction's pole count.
    std::optional<int> u_degree{};
    std::optional<int> v_degree{};
    // Each axis defaults to uniform clamped knots until a custom vector is supplied.
    std::optional<std::vector<geometry::BSplineKnot>> u_knots{};
    std::optional<std::vector<geometry::BSplineKnot>> v_knots{};
};

[[nodiscard]] geometry::BSplineSurfaceDefinition make_saddle_surface();
[[nodiscard]] geometry::BSplineSurfaceDefinition make_bezier_nurbs_surface(const geometry::CubicBezierPatch::ControlNet &net);
[[nodiscard]] auto make_planar_nurbs_surface(const PlanarSurfaceParameters &parameters)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>;
} // namespace occt_lab
