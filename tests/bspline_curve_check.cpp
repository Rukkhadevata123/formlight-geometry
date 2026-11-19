#include "application/editable_curve.hpp"
#include "geometry/bspline_curve.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <print>
#include <stdexcept>

namespace
{
using occt_lab::geometry::sample_clamped_cubic_bspline;
using occt_lab::geometry::Vec3;

bool near(const Vec3 &actual, const Vec3 &expected)
{
    constexpr double tolerance = 1e-12;
    return std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= tolerance;
}

void check_curve_samples()
{
    constexpr std::array poles{
        Vec3{.x = -3.0, .y = 1.0, .z = 0.0}, Vec3{.x = -2.0, .y = 2.0, .z = 1.0}, Vec3{.x = 1.0, .y = 4.0, .z = 3.0},
        Vec3{.x = 3.0, .y = -2.0, .z = 1.0}, Vec3{.x = 6.0, .y = 0.0, .z = 0.0},
    };
    std::array<Vec3, 3> samples{};
    sample_clamped_cubic_bspline(poles, samples);

    if (!near(samples.front(), poles.front()) || !near(samples.back(), poles.back()))
    {
        throw std::runtime_error("The clamped curve must interpolate both endpoint poles");
    }
    // At the interior knot, the three active basis functions are 1/4, 1/2 and 1/4.
    if (constexpr Vec3 expected_middle{.x = 0.75, .y = 2.0, .z = 2.0}; !near(samples[1], expected_middle))
    {
        throw std::runtime_error("Unexpected cubic B-spline sample at the middle knot");
    }

    constexpr std::array unit_weights{1.0, 1.0, 1.0, 1.0, 1.0};
    std::array<Vec3, 3> weighted{};
    sample_clamped_cubic_bspline(poles, weighted, unit_weights);
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        if (!near(weighted[i], samples[i]))
        {
            throw std::runtime_error("Unit weights must reproduce the polynomial B-spline");
        }
    }

    constexpr std::array weights{1.0, 1.0, 3.0, 1.0, 1.0};
    sample_clamped_cubic_bspline(poles, weighted, weights);
    if (constexpr Vec3 rational_middle{.x = 0.875, .y = 3.0, .z = 2.5}; !near(weighted[1], rational_middle) ||
                                                                        !near(weighted.front(), poles.front()) ||
                                                                        !near(weighted.back(), poles.back()))
    {
        throw std::runtime_error("Unexpected rational midpoint or clamped endpoints");
    }

    constexpr std::array scaled_weights{2.0, 2.0, 6.0, 2.0, 2.0};
    sample_clamped_cubic_bspline(poles, samples, scaled_weights);
    if (!near(samples[1], weighted[1]))
    {
        throw std::runtime_error("Common scaling of all weights changed the rational midpoint");
    }
}

void check_curve_edits()
{
    EditableCurve curve;
    const auto original = curve.current();
    auto edited = original.definition;
    edited.poles.front().z = 18.0;
    edited.poles[2].y += 10.0;
    constexpr auto middle = original.samples.size() / 2;
    if (!curve.apply(edited) || curve.current().definition != edited ||
        !near(curve.current().samples.front(), edited.poles.front()) ||
        near(curve.current().samples[middle], original.samples[middle]))
    {
        throw std::runtime_error("An applied edit must replace the poles and sampled curve together");
    }
    const auto unit_samples = curve.current().samples;
    const auto unit_definition = edited;
    curve.undo();
    if (curve.current().definition != original.definition || curve.current().samples != original.samples)
    {
        throw std::runtime_error("Undo failed to restore the original control points and samples");
    }
    curve.redo();
    if (curve.current().definition != unit_definition || curve.current().samples != unit_samples)
    {
        throw std::runtime_error("Redo failed to restore the edited control points and samples");
    }

    edited.weights[2] = 4.0;
    if (!curve.apply(edited) || curve.current().definition != edited ||
        curve.current().unit_weight_samples != unit_samples ||
        near(curve.current().samples[middle], unit_samples[middle]))
    {
        throw std::runtime_error("A weight-only edit must retain the unit curve for the same poles");
    }
    const auto weighted_samples = curve.current().samples;
    curve.undo();
    if (!curve.can_redo() || curve.can_undo() || curve.current().definition != unit_definition ||
        curve.current().samples != unit_samples || curve.current().unit_weight_samples)
    {
        throw std::runtime_error("Undo failed to restore the unit-weight curve");
    }
    auto invalid = edited;
    invalid.poles[1].x = std::numeric_limits<double>::infinity();
    if (curve.apply(invalid))
    {
        throw std::runtime_error("An infinite pole coordinate was accepted");
    }
    invalid = edited;
    for (const double weight :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        invalid.weights[2] = weight;
        if (curve.apply(invalid))
        {
            throw std::runtime_error("A non-positive or non-finite weight was accepted");
        }
    }
    if (!curve.apply(unit_definition) || !curve.can_redo() || curve.current().definition != unit_definition ||
        curve.current().samples != unit_samples || curve.current().unit_weight_samples)
    {
        throw std::runtime_error("Failed or unchanged edits must preserve the curve and redo entry");
    }
    curve.redo();
    if (curve.current().definition != edited || curve.current().samples != weighted_samples ||
        curve.current().unit_weight_samples != unit_samples)
    {
        throw std::runtime_error("Redo failed to restore the edited curve");
    }
    curve.undo();
    edited.poles.back().z = -5.0;
    if (!curve.apply(edited) || curve.can_redo() || !curve.can_undo())
    {
        throw std::runtime_error("A replacement edit must start a new history branch");
    }
}

void check_knot_insertion(const bool rational)
{
    EditableCurve curve;
    auto definition = curve.current().definition;
    definition.weights.assign(definition.poles.size(), 2.0);
    if (rational)
    {
        definition.weights[2] = 8.0;
    }
    if (!curve.apply(definition))
    {
        throw std::runtime_error("Failed to prepare the knot insertion fixture");
    }
    const auto original = curve.current();
    constexpr double parameter = 0.25;
    if (!curve.insert_knot(parameter))
    {
        throw std::runtime_error("An interior knot could not be inserted");
    }
    const auto refined = curve.current();
    const auto &knots = refined.definition.knots;
    if (refined.definition.poles.size() != original.definition.poles.size() + 1 ||
        knots.size() != original.definition.knots.size() + 1 || knots[1].value != parameter ||
        knots[1].multiplicity != 1 || knots.front() != definition.knots.front() ||
        knots.back() != definition.knots.back())
    {
        throw std::runtime_error("Insertion must add one simple interior knot and one pole");
    }
    if (!refined.refinement || refined.refinement->max_sample_deviation > 1e-12 ||
        refined.framing_extent != original.framing_extent || refined.unit_weight_samples.has_value() != rational ||
        refined.definition.weights.front() != definition.weights.front())
    {
        throw std::runtime_error("Insertion changed the sampled shape, framing or weight scale");
    }
    for (std::size_t i = 0; i < original.samples.size(); ++i)
    {
        if (!near(original.samples[i], refined.samples[i]))
        {
            throw std::runtime_error("The refined curve moved at a shared parameter");
        }
    }

    curve.undo();
    for (const double invalid : {0.0, 1.0, -0.25, 1.25, std::numeric_limits<double>::quiet_NaN()})
    {
        if (curve.insert_knot(invalid) || !curve.can_redo() || curve.current().definition != definition)
        {
            throw std::runtime_error("Invalid insertion must preserve the original definition and redo entry");
        }
    }
    curve.redo();
    if (curve.current().definition != refined.definition || curve.current().samples != refined.samples ||
        curve.current().refinement != refined.refinement)
    {
        throw std::runtime_error("Redo must restore the complete refined curve");
    }

    for (int multiplicity = 2; multiplicity <= 3; ++multiplicity)
    {
        if (!curve.insert_knot(parameter) || curve.current().definition.knots.size() != knots.size() ||
            curve.current().definition.knots[1].multiplicity != multiplicity)
        {
            throw std::runtime_error("Inserting an existing knot must increase its multiplicity");
        }
    }
    const auto saturated = curve.current().definition;
    if (curve.insert_knot(parameter) || curve.current().definition != saturated)
    {
        throw std::runtime_error("Inserting a saturated knot must preserve the cubic definition");
    }
    auto edited = saturated;
    edited.poles.back().z += 5.0;
    if (!curve.apply(edited) || curve.current().definition.knots != saturated.knots ||
        !near(curve.current().samples.back(), edited.poles.back()) || curve.current().refinement)
    {
        throw std::runtime_error("Pole editing must use the refined knots and clear the old deviation");
    }
}

void check_degree_elevation(const bool rational)
{
    EditableCurve curve;
    if (rational)
    {
        auto weighted = curve.current().definition;
        weighted.weights.assign(weighted.poles.size(), 2.0);
        weighted.weights[2] = 8.0;
        if (!curve.apply(weighted) || !curve.insert_knot(0.25))
        {
            throw std::runtime_error("Failed to prepare the nonuniform rational curve");
        }
    }

    const auto original = curve.current();
    if (!curve.elevate_degree(4))
    {
        throw std::runtime_error("Failed to elevate the cubic curve to degree four");
    }
    const auto elevated = curve.current();
    const auto &before = original.definition;
    const auto &after = elevated.definition;
    const auto expected_poles = before.poles.size() + before.knots.size() - 1;
    if (after.degree != 4 || after.poles.size() != expected_poles || after.knots.size() != before.knots.size())
    {
        throw std::runtime_error("Unexpected degree, pole count or knot count after elevation");
    }
    for (std::size_t i = 0; i < before.knots.size(); ++i)
    {
        if (after.knots[i].value != before.knots[i].value ||
            after.knots[i].multiplicity != before.knots[i].multiplicity + 1)
        {
            throw std::runtime_error("Degree elevation must preserve knot values and increase each multiplicity");
        }
    }
    if (!elevated.refinement || elevated.refinement->previous_degree != before.degree ||
        elevated.refinement->previous_pole_count != before.poles.size() ||
        elevated.refinement->max_sample_deviation > 1e-12 || elevated.framing_extent != original.framing_extent ||
        elevated.unit_weight_samples.has_value() != rational ||
        std::abs(after.weights.front() - before.weights.front()) > 1e-12)
    {
        throw std::runtime_error("Degree elevation lost the comparison, framing or weight scale");
    }
    for (std::size_t i = 0; i < original.samples.size(); ++i)
    {
        if (!near(elevated.samples[i], original.samples[i]))
        {
            throw std::runtime_error("Degree elevation changed a position at a shared parameter");
        }
    }

    curve.undo();
    for (const int invalid : {2, 3, std::numeric_limits<int>::max()})
    {
        if (curve.elevate_degree(invalid) || !curve.can_redo() || curve.current().definition != before ||
            curve.current().refinement != original.refinement)
        {
            throw std::runtime_error("Invalid elevation changed the curve or discarded redo");
        }
    }
    curve.redo();
    if (curve.current().definition != after || curve.current().samples != elevated.samples ||
        curve.current().refinement != elevated.refinement)
    {
        throw std::runtime_error("Redo must restore the elevated curve and its comparison");
    }
    const auto knot = after.knots[1];
    for (int multiplicity = knot.multiplicity + 1; multiplicity <= after.degree; ++multiplicity)
    {
        if (!curve.insert_knot(knot.value) || curve.current().definition.knots[1].multiplicity != multiplicity)
        {
            throw std::runtime_error("Knot insertion must use the elevated degree as its multiplicity limit");
        }
    }
    if (curve.insert_knot(knot.value) || !curve.elevate_degree(5))
    {
        throw std::runtime_error("The saturated quartic curve must reject another insertion and allow elevation");
    }
    auto edit = curve.current().definition;
    edit.poles.back().z += 5.0;
    if (!curve.apply(edit) || curve.current().definition.degree != 5 ||
        !near(curve.current().samples.back(), edit.poles.back()) || curve.current().refinement)
    {
        throw std::runtime_error("Pole edits must use the elevated degree and clear the comparison");
    }
}
void check_interpolation()
{
    constexpr std::array data{Vec3{.x = 0.0, .y = 0.0, .z = 0.0}, Vec3{.x = 1.0, .y = 0.0, .z = 0.0},
                              Vec3{.x = 1.0, .y = 2.0, .z = 0.0}, Vec3{.x = 1.0, .y = 2.0, .z = 1.0}};
    constexpr std::array parameters{0.0, 0.25, 0.75, 1.0};
    EditableCurve curve;
    auto source = occt_lab::geometry::make_uniform_cubic(data);
    source.weights[1] = 4.0;
    if (!curve.apply(source) || !curve.interpolate_through_poles())
    {
        throw std::runtime_error("Failed to interpolate four distinct 3D data points");
    }
    const auto interpolated = curve.current();
    if (interpolated.interpolation_points.size() != data.size() || interpolated.unit_weight_samples)
    {
        throw std::runtime_error("Interpolation must retain the data points and solve for its own curve");
    }
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        const auto &[point, parameter, residual] = interpolated.interpolation_points[i];
        const auto sample = static_cast<std::size_t>(parameters[i] * (interpolated.samples.size() - 1));
        if (point != data[i] || parameter != parameters[i] || residual > 1e-12 ||
            !near(interpolated.samples[sample], data[i]))
        {
            throw std::runtime_error("Interpolated positions must pass through each input at its chord parameter");
        }
    }
    curve.undo();
    if (curve.current().definition != source || !curve.current().interpolation_points.empty())
    {
        throw std::runtime_error("Undo must restore the weighted source curve");
    }
    curve.redo();
    if (curve.current().interpolation_points != interpolated.interpolation_points ||
        curve.current().definition != interpolated.definition)
    {
        throw std::runtime_error("Redo must restore the interpolant and its residuals");
    }
    if (!curve.insert_knot(0.5) || curve.current().interpolation_points.size() != data.size())
    {
        throw std::runtime_error("Knot insertion lost the interpolation data");
    }
    for (const auto &point : curve.current().interpolation_points)
    {
        if (point.residual > 1e-12)
        {
            throw std::runtime_error("The refined curve no longer passes through the input points");
        }
    }
    auto edited = curve.current().definition;
    edited.poles.front().z += 1.0;
    if (!curve.apply(edited) || !curve.current().interpolation_points.empty())
    {
        throw std::runtime_error("A pole edit must clear the previous interpolation report");
    }
}

void check_interpolation_failure()
{
    EditableCurve curve;
    const auto original = curve.current().definition;
    auto duplicate = original;
    duplicate.poles[1] = duplicate.poles.front();
    if (!curve.apply(duplicate) || !curve.apply(original))
    {
        throw std::runtime_error("Could not prepare a curve with coincident adjacent poles");
    }
    curve.undo();
    if (curve.interpolate_through_poles() || curve.current().definition != duplicate || !curve.can_redo())
    {
        throw std::runtime_error("Failed interpolation must preserve the source curve and redo entry");
    }
}

void check_off_grid_residual()
{
    constexpr std::array data{Vec3{.x = 0.0, .y = 0.0, .z = 0.0}, Vec3{.x = 1.0, .y = 0.0, .z = 0.0},
                              Vec3{.x = 4.0, .y = 0.0, .z = 0.0}, Vec3{.x = 7.0, .y = 0.0, .z = 0.0}};
    const auto curve = occt_lab::geometry::interpolate_curve(data);
    const auto &input = curve.interpolation_points.at(1);
    // u = 1/7 lies between the 129 display samples.
    if (std::abs(input.parameter - 1.0 / 7.0) > 1e-14 || input.residual > 1e-12)
    {
        throw std::runtime_error("Residuals must be evaluated at the original chord parameters");
    }
}
} // namespace

int main()
{
    try
    {
        check_curve_samples();
        check_curve_edits();
        check_knot_insertion(false);
        check_knot_insertion(true);
        check_degree_elevation(false);
        check_degree_elevation(true);
        check_interpolation();
        check_interpolation_failure();
        check_off_grid_residual();
        std::println("B-spline sampling, editing, refinement, interpolation and history verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_curve_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
