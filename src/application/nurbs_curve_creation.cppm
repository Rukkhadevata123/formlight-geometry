export module occt_lab.application.nurbs_curve_creation;

import std;
import occt_lab.geometry.curves.bspline_curve;

export struct LineCurveParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    int pole_count = 6;
    int degree = 3;
    std::optional<std::vector<occt_lab::geometry::BSplineKnot>> knots;
};

namespace occt_lab
{
export [[nodiscard]] geometry::CurveDefinition make_line_curve(const LineCurveParameters &parameters)
{
    const auto &[count, degree, knots] = parameters;
    if (count < LineCurveParameters::min_poles || count > LineCurveParameters::max_poles)
    {
        throw std::invalid_argument("Control point count is outside the supported creation range.");
    }
    constexpr double length_mm = 90.0;
    std::vector<geometry::Vec3> poles;
    poles.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const double x = std::lerp(-length_mm / 2, length_mm / 2, static_cast<double>(i) / (count - 1));
        poles.push_back({.x = x, .y = 0, .z = 0});
    }
    return geometry::make_clamped_curve(poles, degree, knots);
}

export [[nodiscard]] geometry::CurveDefinition make_cubic_bezier_curve()
{
    constexpr std::array<geometry::Vec3, 4> poles{{
        {.x = -45, .y = -12, .z = 0},
        {.x = -20, .y = 42, .z = 0},
        {.x = 15, .y = 34, .z = 0},
        {.x = 45, .y = -8, .z = 0},
    }};
    return geometry::make_uniform_clamped_curve(poles, 3);
}
} // namespace occt_lab
