#pragma once

#include "geometry/curves/bspline_curve.hpp"

struct LineCurveParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    [[nodiscard]] static int degree_limit(int pole_count);
    int pole_count = 6;
    int degree = 3;
};

namespace occt_lab
{
[[nodiscard]] geometry::CurveDefinition make_line_curve(LineCurveParameters parameters);
[[nodiscard]] geometry::CurveDefinition make_cubic_bezier_curve();
} // namespace occt_lab
