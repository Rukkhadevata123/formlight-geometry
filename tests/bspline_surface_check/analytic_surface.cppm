export module formlight_geometry.test.bspline_surface_check.analytic_surface;

import std;
import formlight_geometry.geometry.surfaces.bspline_surface;

namespace formlight_geometry::test
{
export [[nodiscard]] geometry::BSplineSurfaceDefinition make_analytic_surface()
{
    geometry::BSplineSurfaceDefinition definition{
        .u = {.degree = 2, .pole_count = 4, .knots = {{2, 3}, {3, 1}, {5, 3}}},
        .v = {.degree = 1, .pole_count = 3, .knots = {{-2, 2}, {0.5, 1}, {2, 2}}},
        .poles = {},
    };
    // For quadratic B-splines, consecutive interior entries a,b of the expanded knot vector
    // give the pole coefficients (a+b)/2 for u and a*b for u^2.
    constexpr std::array linear_u{2.0, 2.5, 4.0, 5.0};
    constexpr std::array quadratic_u{4.0, 6.0, 15.0, 25.0};
    for (const double v : {-2.0, 0.5, 2.0})
    {
        for (std::size_t i = 0; i < linear_u.size(); ++i)
        {
            definition.poles.push_back(
                {.x = linear_u[i], .y = v, .z = quadratic_u[i] + linear_u[i] * v + std::max(0.0, v - 0.5)});
        }
    }
    definition.weights.assign(definition.poles.size(), 1.0);
    definition.weights[6] = 7.0; // Third pole in the second row.
    return definition;
}

export void check_refined_positions(const geometry::BSplineSurface &before, const geometry::BSplineSurface &after)
{
    const auto bounds = before.domain();
    for (int row = 0; row <= 32; ++row)
    {
        for (int column = 0; column <= 48; ++column)
        {
            const double u = std::lerp(bounds.u_min, bounds.u_max, column / 48.0);
            const double v = std::lerp(bounds.v_min, bounds.v_max, row / 32.0);
            const auto a = before.position(u, v).value();
            const auto b = after.position(u, v).value();
            if (std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) > 1e-11)
            {
                throw std::runtime_error(std::format("Refinement changed the rational surface at u={}, v={}", u, v));
            }
        }
    }
}
} // namespace formlight_geometry::test
