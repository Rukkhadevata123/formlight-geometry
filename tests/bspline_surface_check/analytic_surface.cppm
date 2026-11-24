export module occt_lab.test.bspline_surface_check.analytic_surface;

import std;
import occt_lab.geometry.surfaces.bspline_surface;

namespace occt_lab::test
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
} // namespace occt_lab::test
