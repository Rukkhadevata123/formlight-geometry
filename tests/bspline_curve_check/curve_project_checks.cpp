#include "application/editable_curve.hpp"
#include "document/curve_project.hpp"
#include "../support/project_test_directory.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
namespace fs = std::filesystem;
using namespace occt_lab::document;

void check_custom_curve(const fs::path &path)
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
        if (std::abs(x - (-45.0 + 18.0 * static_cast<double>(i))) > 1e-12 || y != 0 || z != 0)
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
    if (curve.current().samples == line.samples || !save_curve_project(path, curve.current(), {}))
    {
        throw std::runtime_error("Moving the middle pole must bend the new curve and allow saving");
    }
    const auto reopened = open_curve_project(path);
    if (!reopened || reopened->curve != curve.current())
    {
        throw std::runtime_error("The custom curve changed after reopening");
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

void check_bezier_preset(const fs::path &path)
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
    if (std::hypot(midpoint.x + 1.875, midpoint.y - 26.0, midpoint.z) > 1e-12)
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
    if (!save_curve_project(path, curve.current(), {}))
    {
        throw std::runtime_error("Could not save the edited Bezier preset");
    }
    const auto reopened = open_curve_project(path);
    if (!reopened || reopened->curve != curve.current())
    {
        throw std::runtime_error("The weighted Bezier curve changed after reopening");
    }
}

void check_control_point_curve(const fs::path &path)
{
    using namespace occt_lab::geometry;
    constexpr std::array<Vec3, 4> points{{{0, 0, 0}, {8, 0, 8}, {8, 16, 8}, {16, 16, 0}}};
    EditableCurve editor;
    editor.create_interpolated_curve(points).value();
    const auto interpolant = editor.current();

    auto candidate = sample_curve(make_uniform_clamped_curve(points, 3));
    // Cubic Bernstein coefficients at 1/2 are (1, 3, 3, 1) / 8.
    const auto midpoint = candidate.samples[candidate.samples.size() / 2];
    if (std::hypot(midpoint.x - 8, midpoint.y - 8, midpoint.z - 6) > 1e-12 ||
        !std::ranges::equal(candidate.definition.poles, points) ||
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
    save_curve_project(path, editor.current(), {}).value();
    if (open_curve_project(path).value().curve != created)
    {
        throw std::runtime_error("Redo or reopening changed the curve created from control points");
    }

    const auto quadratic = sample_curve(make_uniform_clamped_curve(points, 2));
    const std::vector<BSplineKnot> expected_knots{{0, 3}, {0.5, 1}, {1, 3}};
    if (quadratic.definition.knots != expected_knots || quadratic.samples == created.samples)
    {
        throw std::runtime_error("Changing the degree must rebuild the clamped knots for the same polygon");
    }
}

void check_invalid_file(const fs::path &path, const std::string &original, const std::string_view part,
                        const std::string_view replacement)
{
    auto text = original;
    text.replace(text.find(part), part.size(), replacement);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << text;
    out.close();
    if (open_curve_project(path))
    {
        throw std::runtime_error(std::format("Accepted invalid project field '{}'", part));
    }
}
} // namespace

void check_curve_projects()
{
    const ProjectTestDirectory directory;
    check_custom_curve(directory.path / "custom.occtlab");
    check_bezier_preset(directory.path / "bezier.occtlab");
    check_control_point_curve(directory.path / "control-points.occtlab");
    const auto path = directory.path / u8"曲线.occtlab";
    EditableCurve curve;
    auto definition = curve.current().definition;
    definition.weights[2] = 3.125;
    definition.poles[1].z = 0.12345678901234567;
    const CurveProjectSettings settings{.object_id = 7,
                                        .name = "A \"weighted\" curve",
                                        .pole_number = 2,
                                        .knot_parameter = 0.45,
                                        .fit_tolerance = 0.25};
    if (!curve.apply(definition) || !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not save the weighted test project");
    }
    const auto restored = open_curve_project(path);
    if (!restored || restored->curve.definition != definition || restored->curve.samples != curve.current().samples ||
        restored->settings != settings)
    {
        throw std::runtime_error("Weighted definition or editor settings changed on reopening");
    }
    if (!curve.interpolate_through_poles() || !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not save the interpolated test curve");
    }
    const auto interpolation = open_curve_project(path);
    if (!interpolation || interpolation->curve.data_points != curve.current().data_points ||
        interpolation->curve.approximation)
    {
        throw std::runtime_error("Reopening the interpolant changed its fitting data or mode");
    }
    if (!curve.approximate_data_points(1.0) || !curve.insert_knot(0.45) ||
        !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not replace the saved project with a refined fit");
    }
    auto bad_settings = settings;
    bad_settings.pole_number = 0;
    if (save_curve_project(path, curve.current(), bad_settings))
    {
        throw std::runtime_error("Save accepted an invalid selected pole");
    }
    auto incomplete = curve.current();
    incomplete.definition.weights.pop_back();
    if (save_curve_project(path, incomplete, settings))
    {
        throw std::runtime_error("An incomplete weighted curve replaced the existing file");
    }
    const auto fit = open_curve_project(path);
    const auto &before = curve.current();
    if (!fit || fit->curve.definition != before.definition || fit->curve.framing_extent != before.framing_extent ||
        fit->curve.refinement != before.refinement || fit->curve.approximation != before.approximation ||
        fit->curve.data_points != before.data_points)
    {
        throw std::runtime_error("Reopening the refined fit lost its source points, reference or report");
    }
    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const auto invalid = directory.path / "invalid.occtlab";
    check_invalid_file(invalid, text, "occt-lab-curve 1", "occt-lab-curve 2");
    check_invalid_file(invalid, text, "units mm", "units inch");
    check_invalid_file(invalid, text, "degree 3", "degree 0");
    check_invalid_file(invalid, text, "end\n", "");
    check_invalid_file(invalid, text, "end\n", "end\nextra");
}
