#include "nurbs_surface_creation.hpp"

#include <Precision.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace occt_lab
{
using geometry::BSplineSurfaceDefinition;

int PlanarSurfaceParameters::degree_limit(const int pole_count)
{
    return pole_count > 1 ? std::min(pole_count - 1, Geom_BSplineSurface::MaxDegree()) : 0;
}

namespace
{
auto make_planar_axis(const char *direction, const int poles, const std::optional<int> requested_degree,
                      const std::optional<std::vector<geometry::BSplineKnot>> &custom_knots)
    -> std::expected<geometry::BSplineSurfaceAxis, std::string>
{
    const int limit = PlanarSurfaceParameters::degree_limit(poles);
    const int degree = requested_degree.value_or(std::min(PlanarSurfaceParameters::preferred_degree, limit));
    if (degree < 1 || degree > limit)
    {
        return std::unexpected(std::format("With {} {} poles, the {} degree must be between 1 and {}.", poles,
                                           direction, direction, limit));
    }
    if (custom_knots)
    {
        if (const auto valid = geometry::validate_clamped_knots(*custom_knots, degree, poles); !valid)
        {
            return std::unexpected(std::format("{} knots: {}", direction, valid.error()));
        }
    }
    return geometry::BSplineSurfaceAxis{.degree = degree,
                                        .pole_count = poles,
                                        .knots = custom_knots ? *custom_knots
                                                              : geometry::make_uniform_clamped_knots(poles, degree)};
}
} // namespace

auto make_planar_nurbs_surface(const PlanarSurfaceParameters &parameters)
    -> std::expected<BSplineSurfaceDefinition, std::string>
{
    const auto &[columns, rows, width, height, u_degree, v_degree, u_knots, v_knots] = parameters;
    const auto valid_count = [](int count)
    { return count >= PlanarSurfaceParameters::min_poles && count <= PlanarSurfaceParameters::max_poles; };
    if (!valid_count(columns) || !valid_count(rows))
    {
        return std::unexpected(std::format("Choose {} to {} poles per direction.", PlanarSurfaceParameters::min_poles,
                                           PlanarSurfaceParameters::max_poles));
    }
    const auto valid_length = [](double mm)
    { return std::isfinite(mm) && mm > Precision::Confusion() && !Precision::IsInfinite(mm); };
    if (!valid_length(width) || !valid_length(height))
    {
        return std::unexpected(std::format("Width and height must be greater than {:g} mm and below {:g} mm.",
                                           Precision::Confusion(), 0.5 * Precision::Infinite()));
    }

    auto u = make_planar_axis("U", columns, u_degree, u_knots);
    if (!u)
    {
        return std::unexpected(u.error());
    }
    auto v = make_planar_axis("V", rows, v_degree, v_knots);
    if (!v)
    {
        return std::unexpected(v.error());
    }
    BSplineSurfaceDefinition definition{.u = std::move(*u), .v = std::move(*v)};
    const auto count = static_cast<std::size_t>(rows) * columns;
    definition.poles.reserve(count);
    definition.weights.assign(count, 1.0);
    for (int row = 0; row < rows; ++row)
    {
        const double y = std::lerp(-height / 2, height / 2, static_cast<double>(row) / (rows - 1));
        for (int column = 0; column < columns; ++column)
        {
            const double x = std::lerp(-width / 2, width / 2, static_cast<double>(column) / (columns - 1));
            definition.poles.push_back({.x = x, .y = y, .z = 0});
        }
    }
    return definition;
}

BSplineSurfaceDefinition make_bezier_nurbs_surface(const geometry::CubicBezierPatch::ControlNet &net)
{
    const geometry::BSplineSurfaceAxis cubic{.degree = 3, .pole_count = 4, .knots = {{0, 4}, {1, 4}}};
    return {.u = cubic, .v = cubic, .poles = {net.begin(), net.end()}, .weights = std::vector<double>(net.size(), 1.0)};
}

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
} // namespace occt_lab
