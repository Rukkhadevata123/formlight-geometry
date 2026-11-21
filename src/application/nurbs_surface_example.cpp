#include "nurbs_surface_example.hpp"

#include <algorithm>
#include <array>

namespace occt_lab
{
using geometry::BSplineSurfaceDefinition;

BSplineSurfaceDefinition make_saddle_surface()
{
    const geometry::BSplineSurfaceAxis axis{.degree = 2, .pole_count = 3, .knots = {{0, 3}, {1, 3}}};
    BSplineSurfaceDefinition definition{.u = axis, .v = axis};
    // In the quadratic Bernstein basis, (2t-1)^2 has coefficients 1, -1, 1.
    constexpr std::array square{1.0, -1.0, 1.0};
    for (int v = 0; v < axis.pole_count; ++v)
    {
        for (int u = 0; u < axis.pole_count; ++u)
        {
            definition.poles.push_back({.x = 45.0 * (u - 1), .y = 36.0 * (v - 1), .z = 12.0 * (square[u] - square[v])});
            definition.weights.push_back(u == 1 && v == 1 ? 2.0 : 1.0);
        }
    }
    return definition;
}

BSplineSurfaceDefinition make_nurbs_surface_example()
{
    BSplineSurfaceDefinition definition{
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
} // namespace occt_lab
