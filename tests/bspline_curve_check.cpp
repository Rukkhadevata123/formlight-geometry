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
} // namespace

int main()
{
    try
    {
        check_curve_samples();
        check_curve_edits();
        std::println("Polynomial and rational B-spline samples, weight edits and history verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_curve_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
