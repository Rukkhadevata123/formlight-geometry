#include "geometry/surfaces/bspline_surface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <limits>
#include <print>
#include <stdexcept>
#include <string_view>

namespace
{
using namespace occt_lab::geometry;

BSplineSurfaceDefinition example_surface()
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
    return definition;
}

void check_vector(const Vec3 &actual, const Vec3 &expected, const std::string_view quantity, const double u,
                  const double v)
{
    if (!(std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= 1e-10))
    {
        throw std::runtime_error(
            std::format("Unexpected {} at ({}, {}): ({}, {}, {})", quantity, u, v, actual.x, actual.y, actual.z));
    }
}

void check_evaluation(const BSplineSurface &surface)
{
    const auto [u_min, u_max, v_min, v_max] = surface.domain();
    if (u_min != 2 || u_max != 5 || v_min != -2 || v_max != 2)
    {
        throw std::runtime_error("Surface domain did not match the independent knot intervals");
    }
    for (const double u : {2.0, 2.25, 3.0, 4.5, 5.0})
    {
        for (const double v : {-2.0, -0.75, 0.5, 1.25, 2.0})
        {
            const Vec3 expected{.x = u, .y = v, .z = u * u + u * v + std::max(0.0, v - 0.5)};
            const auto point = surface.position(u, v);
            if (!point)
            {
                throw std::runtime_error(point.error());
            }
            check_vector(*point, expected, "position", u, v);
            const auto sample = surface.evaluate_d1(u, v);
            if (v == 0.5)
            {
                if (sample)
                {
                    throw std::runtime_error("D1 accepted a crease with two different V tangents");
                }
                continue;
            }
            if (!sample)
            {
                throw std::runtime_error(sample.error());
            }
            check_vector(sample->position, expected, "D1 position", u, v);
            check_vector(sample->du, {1, 0, 2 * u + v}, "U derivative", u, v);
            check_vector(sample->dv, {0, 1, u + (v > 0.5 ? 1.0 : 0.0)}, "V derivative", u, v);
        }
    }
    for (const auto [u, v] :
         {std::pair{1.0, 0.0}, std::pair{3.0, 2.1}, std::pair{std::numeric_limits<double>::quiet_NaN(), 0.0}})
    {
        if (surface.position(u, v) || surface.evaluate_d1(u, v))
        {
            throw std::runtime_error("Surface accepted an invalid parameter");
        }
    }
}

void check_rejected(const BSplineSurfaceDefinition &definition, const std::string_view field)
{
    try
    {
        const BSplineSurface surface{definition};
    }
    catch (const std::invalid_argument &)
    {
        return;
    }
    throw std::runtime_error(std::format("Accepted invalid surface {}", field));
}

void check_invalid_definitions(BSplineSurfaceDefinition definition)
{
    const auto valid = definition;
    definition.u.degree = 0;
    check_rejected(definition, "degree");
    definition = valid;
    definition.v.knots.front().multiplicity = 1;
    check_rejected(definition, "endpoint multiplicity");
    definition = valid;
    definition.u.knots[1].multiplicity = 2;
    check_rejected(definition, "multiplicity sum");
    definition = valid;
    definition.v.knots[1].value = definition.v.knots.front().value;
    check_rejected(definition, "knot order");
    definition = valid;
    definition.poles.back().z = std::numeric_limits<double>::infinity();
    check_rejected(definition, "coordinate");
    definition.poles.pop_back();
    check_rejected(definition, "control net size");
}
} // namespace

int main()
{
    try
    {
        const auto definition = example_surface();
        check_evaluation(BSplineSurface{definition});
        check_invalid_definitions(definition);
        std::println("B-spline surface: 4x3 control net, parameter bounds, positions, derivatives and crease verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_surface_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
