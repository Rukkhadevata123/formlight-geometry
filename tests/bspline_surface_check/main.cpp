#include "analytic_surface.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <limits>
#include <print>
#include <stdexcept>
#include <string_view>

void check_nurbs_pole_edits();
void check_nurbs_pole_drag();
void check_nurbs_presets();
void check_custom_surface_knots();
void check_nurbs_surface_projects();
void check_bezier_project_import();
void check_nurbs_resampling();
void check_nurbs_reference_grid();

namespace
{
using namespace occt_lab::geometry;

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
            const auto probe = surface.probe(u, v).value();
            check_vector(probe.position, expected, "probe position", u, v);
            const auto sample = surface.evaluate_d1(u, v);
            if (v == 0.5)
            {
                if (sample || probe.normal)
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
            const double nx = -(2 * u + v);
            const double ny = -(u + (v > 0.5 ? 1.0 : 0.0));
            const double length = std::hypot(nx, ny, 1.0);
            check_vector(probe.normal.value(), {nx / length, ny / length, 1.0 / length}, "probe normal", u, v);
        }
    }
    for (const auto [u, v] :
         {std::pair{1.0, 0.0}, std::pair{3.0, 2.1}, std::pair{std::numeric_limits<double>::quiet_NaN(), 0.0}})
    {
        if (surface.position(u, v) || surface.evaluate_d1(u, v) || surface.probe(u, v))
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

void check_rational_surface(BSplineSurfaceDefinition definition)
{
    definition.weights.assign(definition.poles.size(), 1.0);
    check_evaluation(BSplineSurface{definition});

    constexpr std::size_t weighted_pole = 6; // Column 3 in row 2 of the 4x3 net.
    for (const double scale : {1.0, 1e250, 1e-250})
    {
        std::ranges::fill(definition.weights, scale);
        definition.weights[weighted_pole] = 7.0 * scale;
        const BSplineSurface surface{definition};

        // At (3,-0.75) this pole's tensor basis is 1/6. The extra weight adds
        // a = 6*N*M = 1 to the denominator, with a_u = 2 and a_v = 0.8.
        // For unit-weight surface S and pole P, the new point is (S + a*P)/(1+a).
        constexpr double u = 3.0;
        constexpr double v = -0.75;
        constexpr Vec3 expected{3.5, -0.125, 11.875};
        const auto point = surface.position(u, v).value();
        const auto sample = surface.evaluate_d1(u, v).value();
        check_vector(point, expected, "rational position", u, v);
        check_vector(sample.position, expected, "rational D1 position", u, v);
        check_vector(sample.du, {1.0, 0.625, 7.75}, "rational U derivative", u, v);
        check_vector(sample.dv, {0.2, 0.75, 3.55}, "rational V derivative", u, v);
        const auto probe = surface.probe(u, v).value();
        check_vector(probe.position, expected, "rational probe position", u, v);
        constexpr Vec3 cross{-3.59375, -2.0, 0.625};
        const double length = std::hypot(cross.x, cross.y, cross.z);
        check_vector(probe.normal.value(), {cross.x / length, cross.y / length, cross.z / length},
                     "rational probe normal", u, v);
        check_vector(surface.position(5, 2).value(), definition.poles.back(), "clamped corner", 5, 2);
    }
}

void check_preview_isolines(const BSplineSurface &surface)
{
    const auto lines = surface.isolines({.constant_u_lines = 7, .constant_v_lines = 17, .samples_per_line = 49}).value();
    if (lines.constant_u.size() != 7 || lines.constant_v.size() != 17 ||
        lines.constant_u.at(2).size() != 49 || lines.constant_v.at(5).size() != 49)
    {
        throw std::runtime_error("Unexpected NURBS isoline sampling dimensions");
    }
    constexpr Vec3 weighted_point{3.5, -0.125, 11.875};
    check_vector(lines.constant_u.at(2).at(15), weighted_point, "constant U at weighted sample", 3, -0.75);
    check_vector(lines.constant_v.at(5).at(16), weighted_point, "constant V at weighted sample", 3, -0.75);
    check_vector(lines.constant_v.front().front(), {2, -2, 0}, "first corner", 2, -2);
    check_vector(lines.constant_u.back().back(), {5, 2, 36.5}, "last corner", 5, 2);
    check_vector(lines.constant_v.at(10).at(16), {11.0 / 3.0, 0.5, 89.0 / 6.0}, "V crease", 3, 0.5);
    if (surface.isolines({.samples_per_line = 1}))
    {
        throw std::runtime_error("NURBS isolines accepted a single point per line");
    }
}

void check_singular_probe(BSplineSurfaceDefinition definition)
{
    std::ranges::fill(definition.poles, Vec3{0, 0, 0});
    const auto probe = BSplineSurface{definition}.probe(3, 0).value();
    check_vector(probe.position, {0, 0, 0}, "singular probe position", 3, 0);
    if (probe.normal)
    {
        throw std::runtime_error("A collapsed surface produced a probe normal");
    }
}

void check_invalid_weights(BSplineSurfaceDefinition definition)
{
    definition.weights.assign(definition.poles.size() - 1, 1.0);
    check_rejected(definition, "weight count");
    definition.weights.push_back(1.0);
    for (const double invalid :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        definition.weights.back() = invalid;
        check_rejected(definition, "weight");
    }
    definition.weights.front() = 1e-250;
    definition.weights.back() = 1e250;
    check_rejected(definition, "weight ratio");
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
        auto definition = occt_lab::test::make_analytic_surface();
        check_preview_isolines(BSplineSurface{definition});
        definition.weights.clear();
        check_evaluation(BSplineSurface{definition});
        check_invalid_definitions(definition);
        check_rational_surface(definition);
        check_invalid_weights(definition);
        check_nurbs_pole_edits();
        check_nurbs_pole_drag();
        check_nurbs_presets();
        check_custom_surface_knots();
        check_nurbs_surface_projects();
        check_bezier_project_import();
        check_nurbs_resampling();
        check_nurbs_reference_grid();
        check_singular_probe(definition);
        std::println("B-spline surface: 4x3 control net, parameter bounds, positions, derivatives, crease "
                     "and rational weights verified; preview isolines, pole edit history and pole dragging verified; "
                     "saddle preset and custom plane creation verified; independent U/V degrees verified; "
                     "custom knots verified; NURBS project round trips verified; Bezier import verified; "
                     "NURBS resampling verified; NURBS probe verified; Bezier surface presets verified; "
                     "NURBS reference grid verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_surface_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
