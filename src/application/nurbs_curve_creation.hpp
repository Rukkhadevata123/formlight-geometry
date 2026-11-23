#pragma once

#include "geometry/curves/bspline_curve.hpp"

struct LineCurveParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    int pole_count = 6;
    int degree = 3;
    std::optional<std::vector<occt_lab::geometry::BSplineKnot>> knots;
};

namespace occt_lab
{
[[nodiscard]] geometry::CurveDefinition make_line_curve(const LineCurveParameters &parameters);
[[nodiscard]] geometry::CurveDefinition make_cubic_bezier_curve();
} // namespace occt_lab
