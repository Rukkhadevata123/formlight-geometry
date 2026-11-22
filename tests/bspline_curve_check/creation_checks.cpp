#include "../support/near.hpp"
#include "application/editable_curve.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace
{
using occt_lab::test::near;

void check_custom_curve()
{
    EditableCurve curve;
    const auto original = curve.current();
    if (!curve.create_line({}))
    {
        throw std::runtime_error("Could not create the six-pole cubic line");
    }
    const auto line = curve.current();
    const auto &definition = line.definition;
    using occt_lab::geometry::BSplineKnot;
    const std::vector<BSplineKnot> knots{{0, 4}, {1.0 / 3, 1}, {2.0 / 3, 1}, {1, 4}};
    if (definition.degree != 3 || definition.knots != knots || definition.poles.size() != 6 ||
        definition.weights != std::vector<double>(6, 1.0))
    {
        throw std::runtime_error("The new line must have unit weights and uniform clamped knots");
    }
    for (std::size_t i = 0; i < definition.poles.size(); ++i)
    {
        const auto [x, y, z] = definition.poles[i];
        if (!near(x, -45.0 + 18.0 * static_cast<double>(i)) || y != 0 || z != 0)
        {
            throw std::runtime_error("Expected equally spaced control points centered on the X axis");
        }
    }
    curve.undo();
    if (curve.current() != original || curve.create_line({.pole_count = 2, .degree = 2}) ||
        curve.create_line({.pole_count = LineCurveParameters::max_poles + 1}) || !curve.can_redo())
    {
        throw std::runtime_error("Undo and rejected creation must preserve the previous curve and redo");
    }
    curve.redo();
    if (curve.current() != line || !curve.preview_pole_move(2, {.x = -9, .y = 12, .z = 0}))
    {
        throw std::runtime_error("Could not move a control point on the recreated line");
    }
    curve.accept_pole_move();
    if (curve.current().samples == line.samples)
    {
        throw std::runtime_error("Moving the middle pole must bend the new curve when committed");
    }
    curve.undo();
    if (curve.current() != line || !curve.create_line({}) || !curve.can_redo())
    {
        throw std::runtime_error("Recreating an identical line must preserve the undone pole edit");
    }
    if (!curve.create_line({.pole_count = 2, .degree = 1}) || curve.current().definition.poles.size() != 2)
    {
        throw std::runtime_error("Could not create the minimal two-pole linear curve");
    }
}

void check_bezier_preset()
{
    EditableCurve curve;
    if (!curve.interpolate_through_poles())
    {
        throw std::runtime_error("Could not prepare an interpolated curve for preset replacement");
    }
    const auto previous = curve.current();
    if (!curve.create_cubic_bezier())
    {
        throw std::runtime_error("Could not create the cubic Bezier preset");
    }
    const auto bezier = curve.current();
    const auto &definition = bezier.definition;
    const std::vector<occt_lab::geometry::BSplineKnot> knots{{0, 4}, {1, 4}};
    if (definition.degree != 3 || definition.poles.size() != 4 || definition.weights != std::vector<double>(4, 1.0) ||
        definition.knots != knots || !bezier.data_points.empty())
    {
        throw std::runtime_error("Expected a single cubic Bezier span with four unit weights and no fitting data");
    }
    // At u = 1/2 the cubic Bernstein coefficients are (1, 3, 3, 1) / 8.
    const auto midpoint = bezier.samples[bezier.samples.size() / 2];
    if (!near(midpoint, {-1.875, 26.0, 0.0}))
    {
        throw std::runtime_error("The sampled Bezier arch has the wrong midpoint");
    }
    curve.undo();
    if (curve.current() != previous)
    {
        throw std::runtime_error("Undoing preset creation lost the previous curve or its fitting data");
    }
    curve.redo();
    if (curve.current() != bezier)
    {
        throw std::runtime_error("Redoing preset creation changed the Bezier curve");
    }

    auto edited = definition;
    edited.weights[1] = 2.0;
    if (!curve.apply(edited) || curve.current().unit_weight_samples != bezier.samples)
    {
        throw std::runtime_error("Editing a preset weight must retain the polynomial Bezier reference");
    }
}

void check_control_point_curve()
{
    using namespace occt_lab::geometry;
    constexpr std::array<Vec3, 4> points{{{0, 0, 0}, {8, 0, 8}, {8, 16, 8}, {16, 16, 0}}};
    EditableCurve editor;
    editor.create_interpolated_curve(points).value();
    const auto interpolant = editor.current();

    auto candidate = sample_curve(make_uniform_clamped_curve(points, 3));
    // Cubic Bernstein coefficients at 1/2 are (1, 3, 3, 1) / 8.
    const auto midpoint = candidate.samples[candidate.samples.size() / 2];
    if (!near(midpoint, {8, 8, 6}) || !std::ranges::equal(candidate.definition.poles, points) ||
        candidate.definition.weights != std::vector<double>(4, 1) || !candidate.data_points.empty() ||
        candidate.samples == interpolant.samples)
    {
        throw std::runtime_error("The supplied points must form the control polygon of the previewed cubic");
    }
    editor.replace_with_created_curve(std::move(candidate), EditableCurve::Framing::keep_view);
    const auto created = editor.current();
    if (created.framing_extent != interpolant.framing_extent)
    {
        throw std::runtime_error("Accepting picked control points changed the viewport scale");
    }
    editor.undo();
    if (editor.current() != interpolant)
    {
        throw std::runtime_error("Undoing a control polygon must restore the previous interpolation data");
    }
    editor.redo();
    if (editor.current() != created)
    {
        throw std::runtime_error("Redo changed the curve created from control points");
    }

    const auto quadratic = sample_curve(make_uniform_clamped_curve(points, 2));
    const std::vector<BSplineKnot> expected_knots{{0, 3}, {0.5, 1}, {1, 3}};
    if (quadratic.definition.knots != expected_knots || quadratic.samples == created.samples)
    {
        throw std::runtime_error("Changing the degree must rebuild the clamped knots for the same polygon");
    }
}

void check_interpolated_curve_creation()
{
    using occt_lab::geometry::Vec3;
    // Chord lengths of 10, 20 and 10 give known parameters in all three dimensions.
    constexpr std::array<Vec3, 4> points{{{0, 0, 0}, {10, 0, 0}, {10, 0, 20}, {10, -10, 20}}};
    constexpr std::array parameters{0.0, 0.25, 0.75, 1.0};
    EditableCurve curve;
    const auto previous = curve.current();
    if (!curve.create_interpolated_curve(points))
    {
        throw std::runtime_error("Could not interpolate the supplied XYZ rows");
    }
    const auto interpolant = curve.current();
    if (interpolant.data_points.size() != points.size())
    {
        throw std::runtime_error("Creation lost interpolation data points");
    }
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto &[position, parameter, residual] = interpolant.data_points[i];
        const auto sample = static_cast<std::size_t>(parameters[i] * (interpolant.samples.size() - 1));
        if (!near(interpolant.samples[sample], points[i]) || position != points[i] || parameter != parameters[i] ||
            !near(residual, 0.0))
        {
            throw std::runtime_error("The interpolant must pass through each XYZ row at its chord parameter");
        }
    }
    curve.undo();
    if (curve.current() != previous)
    {
        throw std::runtime_error("Undo did not restore the curve preceding XYZ input");
    }
    auto invalid = points;
    invalid[1] = invalid[0];
    if (curve.create_interpolated_curve(invalid) || !curve.can_redo() || curve.current() != previous)
    {
        throw std::runtime_error("Rejecting adjacent duplicate rows must preserve the previous curve and redo");
    }
    curve.redo();
    if (curve.current() != interpolant)
    {
        throw std::runtime_error("Redo lost the interpolant or its data points");
    }

    const auto pair = std::span{points}.first<2>();
    if (!curve.create_interpolated_curve(pair, EditableCurve::Framing::keep_view) ||
        curve.current().framing_extent != interpolant.framing_extent || curve.current().data_points.size() != 2 ||
        curve.current().samples.front() != pair.front() || curve.current().samples.back() != pair.back())
    {
        throw std::runtime_error("Picking two points must preserve the view and interpolate both endpoints");
    }
}
} // namespace

void check_curve_creation()
{
    check_custom_curve();
    check_bezier_preset();
    check_control_point_curve();
    check_interpolated_curve_creation();
}
