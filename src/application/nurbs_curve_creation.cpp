#include "nurbs_curve_creation.hpp"

#include <Geom_BSplineCurve.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>

int LineCurveParameters::degree_limit(const int pole_count)
{
    return pole_count > 1 ? std::min(pole_count - 1, Geom_BSplineCurve::MaxDegree()) : 0;
}

namespace occt_lab
{
geometry::CurveDefinition make_line_curve(const LineCurveParameters parameters)
{
    const auto [count, degree] = parameters;
    if (count < LineCurveParameters::min_poles || count > LineCurveParameters::max_poles)
    {
        throw std::invalid_argument("Control point count is outside the supported creation range.");
    }
    if (degree < 1 || degree > LineCurveParameters::degree_limit(count))
    {
        throw std::invalid_argument("Choose a degree supported by OCCT and smaller than the control point count.");
    }

    constexpr double length_mm = 90.0;
    geometry::CurveDefinition line{.degree = degree};
    line.knots = geometry::make_uniform_clamped_knots(count, degree);
    line.weights.assign(count, 1.0);
    line.poles.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const double x = std::lerp(-length_mm / 2, length_mm / 2, static_cast<double>(i) / (count - 1));
        line.poles.push_back({.x = x, .y = 0, .z = 0});
    }
    return line;
}

geometry::CurveDefinition make_cubic_bezier_curve()
{
    constexpr std::array<geometry::Vec3, 4> poles{{
        {.x = -45, .y = -12, .z = 0},
        {.x = -20, .y = 42, .z = 0},
        {.x = 15, .y = 34, .z = 0},
        {.x = 45, .y = -8, .z = 0},
    }};
    return geometry::make_uniform_cubic(poles);
}
} // namespace occt_lab
