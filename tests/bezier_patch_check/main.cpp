#include "application/editable_patch.hpp"
#include "geometry/surfaces/bezier_patch.hpp"
#include "geometry/surfaces/bspline_surface.hpp"
#include "geometry/surfaces/occt_bezier_patch.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <limits>
#include <numbers>
#include <print>
#include <stdexcept>
#include <vector>

void check_patch_projects();

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::OcctBezierPatch;
using occt_lab::geometry::PatchError;
using occt_lab::geometry::PatchIsolines;
using occt_lab::geometry::PatchSampling;
using occt_lab::geometry::SurfaceSample;
using occt_lab::geometry::Vec3;

constexpr std::size_t side_length = 4;
using ControlNet = std::array<Vec3, side_length * side_length>;

struct ExpectedSample
{
    double u;
    double v;
    Vec3 position;
    Vec3 normal;
};

ControlNet raised_patch()
{
    ControlNet control_points{};
    for (std::size_t row = 0; row < side_length; ++row)
    {
        for (std::size_t column = 0; column < side_length; ++column)
        {
            const bool is_interior = (row == 1 || row == 2) && (column == 1 || column == 2);
            control_points[row * side_length + column] = {
                .x = static_cast<double>(column),
                .y = static_cast<double>(row),
                .z = is_interior ? 0.4 : 0.0,
            };
        }
    }
    return control_points;
}

bool near(const Vec3 &actual, const Vec3 &expected)
{
    constexpr double tolerance = 1e-12;
    return std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= tolerance;
}

void check_samples(const CubicBezierPatch &patch, const OcctBezierPatch &reference)
{
    using namespace occt_lab::geometry;
    const BSplineSurfaceAxis cubic_axis{.degree = 3, .pole_count = 4, .knots = {{0, 4}, {1, 4}}};
    const auto &net = patch.control_net();
    const BSplineSurfaceDefinition definition{
        .u = cubic_axis,
        .v = cubic_axis,
        .poles = {net.begin(), net.end()},
        .weights = std::vector<double>(net.size(), 1.0),
    };
    const BSplineSurface nurbs{definition};

    constexpr Vec3 up{.x = 0.0, .y = 0.0, .z = 1.0};
    // The z slopes relative to x and y at (0.25, 0.75) are +0.1125 and -0.1125.
    constexpr double slope = 0.1125;
    const double normal_length = std::sqrt(1.0 + 2.0 * slope * slope);
    const Vec3 off_center_normal{
        .x = -slope / normal_length,
        .y = slope / normal_length,
        .z = 1.0 / normal_length,
    };
    const std::array samples{
        ExpectedSample{.u = 0.0, .v = 0.0, .position = {.x = 0.0, .y = 0.0, .z = 0.0}, .normal = up},
        ExpectedSample{.u = 0.5, .v = 0.5, .position = {.x = 1.5, .y = 1.5, .z = 0.225}, .normal = up},
        ExpectedSample{.u = 1.0, .v = 1.0, .position = {.x = 3.0, .y = 3.0, .z = 0.0}, .normal = up},
        ExpectedSample{
            .u = 0.25,
            .v = 0.75,
            .position = {.x = 0.75, .y = 2.25, .z = 0.1265625},
            .normal = off_center_normal,
        },
    };

    for (const auto &[u, v, position, normal] : samples)
    {
        const auto sample = patch.evaluate(u, v);
        if (!sample)
        {
            throw std::runtime_error(std::format("Could not evaluate the patch at ({}, {})", u, v));
        }
        if (!near(sample->position, position))
        {
            throw std::runtime_error(std::format("Unexpected position at ({}, {})", u, v));
        }
        if (!near(sample->normal, normal))
        {
            throw std::runtime_error(std::format("Unexpected normal at ({}, {})", u, v));
        }
        const auto nurbs_point = nurbs.position(u, v);
        if (!nurbs_point || !near(*nurbs_point, sample->position))
        {
            throw std::runtime_error(std::format("Unit-weight NURBS and Bernstein positions differ at ({}, {})", u, v));
        }
        const auto occt_sample = reference.evaluate(u, v);
        if (!occt_sample || !near(occt_sample->position, position) || !near(occt_sample->normal, normal))
        {
            throw std::runtime_error(std::format("Unexpected OCCT position or oriented normal at ({}, {})", u, v));
        }
        if (const auto [distance, angle] = compare_surface_samples(*sample, *occt_sample);
            !(distance <= 1e-12 && angle <= 1e-10))
        {
            throw std::runtime_error(std::format("Bezier evaluators differ by {} mm and {} deg", distance, angle));
        }
    }
}

void check_comparison_metrics()
{
    constexpr SurfaceSample up{.position = {}, .normal = {.x = 0.0, .y = 0.0, .z = 1.0}};
    constexpr SurfaceSample down{.position = {.x = 3.0, .y = 4.0, .z = 0.0}, .normal = {.x = 0.0, .y = 0.0, .z = -1.0}};
    if (const auto [distance, angle] = compare_surface_samples(up, down);
        distance != 5.0 || !(std::abs(angle - 180.0) <= 1e-12))
    {
        throw std::runtime_error("Opposite normals must differ by 180 degrees");
    }

    constexpr double small_angle = 1e-10;
    const SurfaceSample tilted{.position = {},
                               .normal = {.x = std::sin(small_angle), .y = 0.0, .z = std::cos(small_angle)}};
    const double degrees = compare_surface_samples(up, tilted).normal_angle_degrees;
    constexpr double expected_degrees = small_angle * (180.0 / std::numbers::pi);
    if (!(std::abs(degrees - expected_degrees) <= 1e-14))
    {
        throw std::runtime_error("A small resolvable normal angle rounded to an incorrect value");
    }
}

void check_patch_creation()
{
    EditablePatch model;
    const auto example = model.current().control_net();
    if (model.can_undo() || model.can_redo() || !model.create_plane() || !model.can_undo() || model.can_redo())
    {
        throw std::runtime_error("Creating a planar patch should add one undo step");
    }
    constexpr Vec3 expected_point{.x = -22.5, .y = 18.0, .z = 0.0};
    constexpr Vec3 up{.x = 0.0, .y = 0.0, .z = 1.0};
    const auto sample = model.current().evaluate(0.25, 0.75);
    const auto reference = model.reference().evaluate(0.25, 0.75);
    if (!sample || !reference || !near(sample->position, expected_point) ||
        !near(reference->position, expected_point) || !near(sample->normal, up) || !near(reference->normal, up))
    {
        throw std::runtime_error("The planar patch and its OCCT reference should agree in XY");
    }
    constexpr PatchSampling sampling{.constant_u_lines = 3, .constant_v_lines = 5, .samples_per_line = 3};
    if (model.create_plane() || !model.resample(sampling))
    {
        throw std::runtime_error("Repeating planar creation should preserve the previous patch");
    }
    model.undo();
    constexpr Vec3 raised_center{.x = 0.0, .y = 0.0, .z = 16.03125};
    if (model.current().control_net() != example || model.can_undo() || !model.can_redo() ||
        model.sampling() != sampling || !near(model.isolines().constant_u.at(1).at(1), raised_center) ||
        !near(model.reference().evaluate(0.5, 0.5).value().position, raised_center))
    {
        throw std::runtime_error("Undo should restore the raised patch and use the current sampling density");
    }
    model.redo();
    if (!model.can_undo() || model.can_redo() || !near(model.isolines().constant_v.at(2).at(1), {}) ||
        !near(model.reference().evaluate(0.5, 0.5).value().normal, up))
    {
        throw std::runtime_error("Redo should restore the planar patch and its derived geometry");
    }
}

void check_reference_failures(const OcctBezierPatch &reference)
{
    if (reference.evaluate(-0.1, 0.5) || reference.evaluate(0.0, std::numeric_limits<double>::quiet_NaN()))
    {
        throw std::runtime_error("OCCT reference accepted invalid patch parameters");
    }
    constexpr ControlNet coincident{};
    if (const auto collapsed = CubicBezierPatch::create(coincident).value();
        OcctBezierPatch{collapsed}.evaluate(0.5, 0.5))
    {
        throw std::runtime_error("OCCT reference returned a normal for a collapsed patch");
    }
}

void check_control_net_edits()
{
    EditablePatch model;
    if (!model.create_plane())
    {
        throw std::runtime_error("Expected a new plane for control point editing");
    }
    const auto plane = model.current().control_net();
    auto draft = plane;
    draft[2 * side_length + 1].z = 32;
    draft[side_length + 2].x += 8;
    draft[side_length + 2].y -= 4;
    if (!model.apply(draft).value() || model.current().control_net() != draft)
    {
        throw std::runtime_error("Applying a multi-pole draft did not commit the control net");
    }

    // At (1/4, 3/4) the two transposed poles have different tensor-product weights.
    constexpr double first_weight = 729.0 / 4096.0;
    constexpr double second_weight = 81.0 / 4096.0;
    constexpr Vec3 expected{.x = -22.5 + 8 * second_weight, .y = 18.0 - 4 * second_weight, .z = 32 * first_weight};
    const auto sample = model.current().evaluate(0.25, 0.75).value();
    const auto reference = model.reference().evaluate(0.25, 0.75).value();
    if (!near(sample.position, expected) || !near(reference.position, expected) ||
        !near(sample.normal, reference.normal) || !near(model.isolines().constant_u.at(2).at(24), expected))
    {
        throw std::runtime_error("An edited pole was transposed or its derived geometry is stale");
    }

    model.undo();
    if (model.current().control_net() != plane || model.apply(plane).value() || !model.can_redo())
    {
        throw std::runtime_error("One undo should restore both edited poles; a no-op must retain redo");
    }
    auto invalid = plane;
    invalid[1] = invalid[0]; // Zero u tangent at (0,0).
    if (model.apply(invalid).has_value())
    {
        throw std::runtime_error("A degenerate corner was accepted");
    }
    for (const double coordinate : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::max()})
    {
        invalid = plane;
        invalid[1].x = coordinate;
        if (model.apply(invalid).has_value())
        {
            throw std::runtime_error("An invalid control point coordinate was accepted");
        }
    }
    if (!model.can_redo() || model.current().control_net() != plane ||
        !near(model.reference().evaluate(0.5, 0.5).value().position, {}) ||
        !near(model.isolines().constant_u.at(4).at(16), {}))
    {
        throw std::runtime_error("A rejected edit changed the plane, its samples, or its history");
    }
    model.redo();
    if (model.current().control_net() != draft || !near(model.isolines().constant_u.at(2).at(24), expected))
    {
        throw std::runtime_error("Redo did not restore the edited surface");
    }
}

void check_pole_drag()
{
    EditablePatch model;
    if (!model.create_plane())
    {
        throw std::runtime_error("Expected planar drag baseline");
    }
    model.undo();
    const auto original = model.current().control_net();
    constexpr std::size_t pole = 2 * side_length + 1;
    const auto move_to = [&](const Vec3 &target)
    {
        if (!model.preview_pole_move(pole, target).value())
        {
            throw std::runtime_error("Expected the pole drag to update the preview");
        }
    };
    auto position = original[pole];
    position.z += 32;
    move_to(position);
    if (model.displayed().control_net()[pole] != position || model.current().control_net() != original ||
        !model.can_redo())
    {
        throw std::runtime_error("A drag preview changed the committed patch or its redo history");
    }
    constexpr Vec3 center{.x = 0, .y = 0, .z = 16.03125 + 4.5};
    if (!near(model.reference().evaluate(0.5, 0.5).value().position, center) ||
        !near(model.isolines().constant_u.at(4).at(16), center))
    {
        throw std::runtime_error("The OCCT or isoline preview did not follow the dragged pole");
    }
    auto invalid = position;
    invalid.x = std::numeric_limits<double>::infinity();
    if (model.preview_pole_move(pole, invalid).has_value() || model.displayed().control_net()[pole] != position)
    {
        throw std::runtime_error("An invalid drag position replaced the last valid preview");
    }
    model.discard_preview();
    if (model.displayed().control_net() != original || !model.can_redo() || model.commit_preview())
    {
        throw std::runtime_error("Cancelling a drag must restore the patch and preserve redo");
    }
    move_to(position);
    move_to(original[pole]);
    if (model.commit_preview() || !model.can_redo())
    {
        throw std::runtime_error("Dragging back to the origin must preserve redo");
    }
    move_to(position);
    position.x += 3;
    move_to(position);
    if (!model.commit_preview() || model.can_redo() || model.current().control_net()[pole] != position)
    {
        throw std::runtime_error("Releasing a drag must commit the final position");
    }
    model.undo();
    if (model.current().control_net() != original)
    {
        throw std::runtime_error("One undo should restore the original control net after multiple drag updates");
    }
    model.redo();
    if (model.current().control_net()[pole] != position)
    {
        throw std::runtime_error("Redo should restore the final dragged position");
    }
}

void check_invalid_inputs(const CubicBezierPatch &patch)
{
    if (const auto sample = patch.evaluate(1.25, 0.5); sample || sample.error() != PatchError::ParameterOutOfRange)
    {
        throw std::runtime_error("Expected an out-of-range parameter error");
    }
    if (const auto sample = patch.evaluate(0.5, std::numeric_limits<double>::quiet_NaN());
        sample || sample.error() != PatchError::NonFiniteParameter)
    {
        throw std::runtime_error("Expected a non-finite parameter error");
    }
    if (const auto empty = CubicBezierPatch::create({}); empty || empty.error() != PatchError::WrongControlPointCount)
    {
        throw std::runtime_error("Expected a control point count error");
    }
}

void check_large_normal()
{
    auto flat = raised_patch();
    for (auto &[x, y, z] : flat)
    {
        x *= 1e100;
        y *= 1e100;
        z = 0.0;
    }
    if (const auto sample = CubicBezierPatch::create(flat).value().evaluate(0.5, 0.5);
        !sample || !near(sample->normal, {.x = 0.0, .y = 0.0, .z = 1.0}))
    {
        throw std::runtime_error("A large finite cross product must still yield a unit normal");
    }
    for (auto &p : flat)
    {
        p.x *= 1e100;
        p.y *= 1e100;
    }
    if (const auto overflow = CubicBezierPatch::create(flat).value().evaluate(0.5, 0.5);
        overflow || overflow.error() != PatchError::NonFiniteEvaluation)
    {
        throw std::runtime_error("An overflowing cross product must not be compared as a valid normal");
    }
}

void check_family(const PatchIsolines::Family &lines, const int line_count, const int samples_per_line,
                  const bool constant_u)
{
    if (lines.size() != static_cast<std::size_t>(line_count))
    {
        throw std::runtime_error("Unexpected number of isolines in the requested direction");
    }
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const double fixed = static_cast<double>(i) / (line_count - 1);
        const auto &line = lines[i];
        if (line.size() != static_cast<std::size_t>(samples_per_line))
        {
            throw std::runtime_error("Unexpected number of samples along the isoline");
        }
        for (std::size_t j = 0; j < line.size(); ++j)
        {
            const double t = static_cast<double>(j) / (samples_per_line - 1);
            // The four raised interior poles contribute (B1 + B2)(u) * (B1 + B2)(v) * 0.4.
            const double z = 3.6 * fixed * (1.0 - fixed) * t * (1.0 - t);
            const double u = constant_u ? fixed : t;
            const double v = constant_u ? t : fixed;
            if (!near(line[j], {.x = 3.0 * u, .y = 3.0 * v, .z = z}))
            {
                throw std::runtime_error(std::format("Unexpected isoline position at line {}, sample {}", i, j));
            }
        }
    }
}

void check_isolines(const CubicBezierPatch &patch)
{
    const auto defaults = sample_patch_isolines(patch);
    if (!defaults)
    {
        throw std::runtime_error("Could not sample the raised patch with the default settings");
    }
    check_family(defaults->constant_u, 9, 33, true);
    check_family(defaults->constant_v, 9, 33, false);

    for (const auto sampling : {PatchSampling{.constant_u_lines = 2, .constant_v_lines = 7, .samples_per_line = 2},
                                PatchSampling{.constant_u_lines = 13, .constant_v_lines = 3, .samples_per_line = 17},
                                PatchSampling{.constant_u_lines = 33, .constant_v_lines = 33, .samples_per_line = 129}})
    {
        const auto lines = sample_patch_isolines(patch, sampling);
        if (!lines)
        {
            throw std::runtime_error("A valid isoline sampling request failed");
        }
        check_family(lines->constant_u, sampling.constant_u_lines, sampling.samples_per_line, true);
        check_family(lines->constant_v, sampling.constant_v_lines, sampling.samples_per_line, false);
    }
    for (const auto invalid : {PatchSampling{.constant_u_lines = 1}, PatchSampling{.constant_v_lines = 34},
                               PatchSampling{.samples_per_line = -1}, PatchSampling{.samples_per_line = 130}})
    {
        if (const auto result = sample_patch_isolines(patch, invalid);
            result || result.error() != PatchError::InvalidSampling)
        {
            throw std::runtime_error("Expected an invalid isoline sampling error");
        }
    }
}
} // namespace

int main()
{
    try
    {
        const auto control_points = raised_patch();
        const auto patch = CubicBezierPatch::create(control_points);
        if (!patch)
        {
            throw std::runtime_error("Could not create the sample Bezier patch");
        }

        const OcctBezierPatch reference{*patch};
        check_samples(*patch, reference);
        check_patch_creation();
        check_control_net_edits();
        check_pole_drag();
        check_patch_projects();
        check_comparison_metrics();
        check_reference_failures(reference);
        check_isolines(*patch);
        check_invalid_inputs(*patch);
        check_large_normal();
        std::println(
            "Bezier patch positions, normals, OCCT comparison, planar creation, control net edits, pole dragging, "
            "undo/redo, adjustable sampling, project round trips and invalid inputs verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bezier_patch_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
