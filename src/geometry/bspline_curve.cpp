#include "bspline_curve.hpp"

#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <Precision.hxx>
#include <gp.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace occt_lab::geometry
{
namespace
{
constexpr int degree = CurveDefinition::degree;

int checked_pole_count(const std::size_t count)
{
    if (count < degree + 1 || count > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("A cubic B-spline requires at least four poles and an OCCT-sized array");
    }
    return static_cast<int>(count);
}

NCollection_Array1<double> normalized_weights(const std::span<const double> weights, const int pole_count)
{
    if (weights.size() != static_cast<std::size_t>(pole_count))
    {
        throw std::invalid_argument("Each B-spline pole requires one weight");
    }
    if (!std::ranges::all_of(weights, [](const double weight) { return std::isfinite(weight) && weight > 0.0; }))
    {
        throw std::invalid_argument("Every B-spline weight must be finite and greater than zero");
    }

    // Common scaling preserves the curve and keeps large weights out of homogeneous products.
    const double largest = std::ranges::max(weights);
    NCollection_Array1<double> normalized{1, pole_count};
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        const double ratio = weights[i] / largest;
        if (ratio <= gp::Resolution())
        {
            throw std::invalid_argument("The weight ratio is too small for OCCT curve evaluation");
        }
        normalized(static_cast<int>(i) + 1) = ratio;
    }
    return normalized;
}

Geom_BSplineCurve make_curve(const CurveDefinition &definition, const bool use_unit_weights = false)
{
    const int pole_count = checked_pole_count(definition.poles.size());
    if (definition.knots.size() < 2 || definition.knots.size() > definition.poles.size())
    {
        throw std::invalid_argument("Invalid number of distinct knots for a clamped cubic B-spline");
    }
    NCollection_Array1<gp_Pnt> poles{1, pole_count};
    for (std::size_t i = 0; i < definition.poles.size(); ++i)
    {
        const auto &[x, y, z] = definition.poles[i];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        {
            throw std::invalid_argument("B-spline poles must have finite coordinates");
        }
        poles(static_cast<int>(i) + 1) = gp_Pnt{x, y, z};
    }

    const int knot_count = static_cast<int>(definition.knots.size());
    NCollection_Array1<double> knots{1, knot_count};
    NCollection_Array1<int> multiplicities{1, knot_count};
    std::int64_t total = 0;
    for (std::size_t i = 0; i < definition.knots.size(); ++i)
    {
        const auto [value, multiplicity] = definition.knots[i];
        const bool endpoint = i == 0 || i + 1 == definition.knots.size();
        if (!std::isfinite(value) || (i > 0 && value <= definition.knots[i - 1].value))
        {
            throw std::invalid_argument("Distinct knots must be finite and strictly increasing");
        }
        const int minimum = endpoint ? degree + 1 : 1;
        const int maximum = endpoint ? degree + 1 : degree;
        if (multiplicity < minimum || multiplicity > maximum)
        {
            throw std::invalid_argument("Invalid knot multiplicity for a clamped cubic B-spline");
        }
        knots(static_cast<int>(i) + 1) = value;
        multiplicities(static_cast<int>(i) + 1) = multiplicity;
        total += multiplicity;
    }
    if (total - degree - 1 != pole_count)
    {
        throw std::invalid_argument("Knot multiplicities do not match the number of poles");
    }
    if (use_unit_weights)
    {
        return Geom_BSplineCurve{poles, knots, multiplicities, degree};
    }
    return Geom_BSplineCurve{poles, normalized_weights(definition.weights, pole_count), knots, multiplicities, degree};
}

void sample_positions(const Geom_BSplineCurve &curve, const std::span<Vec3> samples)
{
    if (samples.size() < 2)
    {
        throw std::invalid_argument("B-spline sampling requires space for both endpoints");
    }
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    const auto intervals = static_cast<double>(samples.size() - 1);
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        const double parameter = std::lerp(first, last, static_cast<double>(index) / intervals);
        const gp_Pnt point = curve.EvalD0(parameter);
        if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()))
        {
            throw std::runtime_error("Curve evaluation exceeded the finite coordinate range");
        }
        samples[index] = Vec3{.x = point.X(), .y = point.Y(), .z = point.Z()};
    }
}
} // namespace

CurveDefinition make_uniform_cubic(const std::span<const Vec3> poles, const std::span<const double> weights)
{
    const int knot_count = checked_pole_count(poles.size()) - degree + 1;
    CurveDefinition definition;
    definition.poles.assign(poles.begin(), poles.end());
    if (weights.empty())
    {
        definition.weights.assign(poles.size(), 1.0);
    }
    else
    {
        definition.weights.assign(weights.begin(), weights.end());
    }
    for (int i = 0; i < knot_count; ++i)
    {
        const bool endpoint = i == 0 || i + 1 == knot_count;
        definition.knots.push_back(
            {.value = static_cast<double>(i) / (knot_count - 1), .multiplicity = endpoint ? degree + 1 : 1});
    }
    return definition;
}

void sample_clamped_cubic_bspline(const std::span<const Vec3> control_points, const std::span<Vec3> samples,
                                  const std::span<const double> weights)
{
    sample_positions(make_curve(make_uniform_cubic(control_points, weights)), samples);
}

SampledCurve sample_curve(const CurveDefinition &definition)
{
    SampledCurve result{.definition = definition};
    sample_positions(make_curve(definition), result.samples);
    const auto &weights = definition.weights;
    if (!std::ranges::all_of(weights, [&weights](const double weight) { return weight == weights.front(); }))
    {
        sample_positions(make_curve(definition, true), result.unit_weight_samples.emplace());
    }
    for (const auto &[x, y, z] : definition.poles)
    {
        result.framing_extent = std::max({result.framing_extent, std::abs(x), std::abs(y), std::abs(z)});
    }
    return result;
}

CurveDefinition insert_cubic_knot(const CurveDefinition &definition, const double parameter)
{
    auto curve = make_curve(definition);
    constexpr double tolerance = Precision::PConfusion();
    if (!std::isfinite(parameter) || parameter - curve.FirstParameter() <= tolerance ||
        curve.LastParameter() - parameter <= tolerance)
    {
        throw std::invalid_argument("Choose an interior knot parameter away from the clamped endpoints");
    }

    const int old_count = curve.NbPoles();
    curve.InsertKnot(parameter, 1, tolerance, true);
    if (curve.NbPoles() == old_count)
    {
        throw std::invalid_argument("This cubic knot already has multiplicity 3");
    }

    // OCCT operates on normalized weights. Restore the user's common factor when reading back the poles.
    const double weight_scale = std::ranges::max(definition.weights);
    CurveDefinition refined;
    for (int i = 1; i <= curve.NbPoles(); ++i)
    {
        const gp_Pnt &point = curve.Pole(i);
        refined.poles.push_back({.x = point.X(), .y = point.Y(), .z = point.Z()});
        refined.weights.push_back(curve.Weight(i) * weight_scale);
    }
    for (int i = 1; i <= curve.NbKnots(); ++i)
    {
        refined.knots.push_back({.value = curve.Knot(i), .multiplicity = curve.Multiplicity(i)});
    }
    return refined;
}
} // namespace occt_lab::geometry
