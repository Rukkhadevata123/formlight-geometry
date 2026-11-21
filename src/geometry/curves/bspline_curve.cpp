#include "occt_bspline.hpp"

#include "geometry/bspline_weights.hpp"
#include "geometry/occt_bspline_knots.hpp"

#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <Precision.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace occt_lab::geometry
{
namespace
{
int checked_pole_count(const std::size_t count, const int degree)
{
    if (degree < 1 || degree > Geom_BSplineCurve::MaxDegree())
    {
        throw std::invalid_argument("Curve degree is outside the supported OCCT range");
    }
    if (count <= static_cast<std::size_t>(degree) || count > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("A B-spline needs more poles than its degree and an OCCT-sized array");
    }
    return static_cast<int>(count);
}

NCollection_Array1<double> normalized_weights(const std::span<const double> weights, const int pole_count)
{
    const double scale = checked_weight_scale(weights, pole_count);
    NCollection_Array1<double> normalized{1, pole_count};
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        normalized(static_cast<int>(i) + 1) = weights[i] / scale;
    }
    return normalized;
}

Geom_BSplineCurve make_curve(const CurveDefinition &definition, const bool use_unit_weights = false)
{
    const int degree = definition.degree;
    const int pole_count = checked_pole_count(definition.poles.size(), degree);
    const auto [knots, multiplicities] = make_clamped_knots(definition.knots, degree, pole_count);
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

CurveDefinition read_bspline_definition(const Geom_BSplineCurve &curve, const double weight_scale)
{
    // Restore the common factor removed when creating the OCCT curve.
    CurveDefinition result{.degree = curve.Degree()};
    for (int i = 1; i <= curve.NbPoles(); ++i)
    {
        const gp_Pnt &point = curve.Pole(i);
        result.poles.push_back({.x = point.X(), .y = point.Y(), .z = point.Z()});
        result.weights.push_back(curve.Weight(i) * weight_scale);
    }
    for (int i = 1; i <= curve.NbKnots(); ++i)
    {
        result.knots.push_back({.value = curve.Knot(i), .multiplicity = curve.Multiplicity(i)});
    }
    return result;
}

CurveDefinition make_uniform_cubic(const std::span<const Vec3> poles, const std::span<const double> weights)
{
    constexpr int degree = 3;
    const int pole_count = checked_pole_count(poles.size(), degree);
    CurveDefinition definition{.degree = degree};
    definition.poles.assign(poles.begin(), poles.end());
    if (weights.empty())
    {
        definition.weights.assign(poles.size(), 1.0);
    }
    else
    {
        definition.weights.assign(weights.begin(), weights.end());
    }
    definition.knots = make_uniform_clamped_knots(pole_count, degree);
    return definition;
}

void sample_clamped_cubic_bspline(const std::span<const Vec3> control_points, const std::span<Vec3> samples,
                                  const std::span<const double> weights)
{
    sample_positions(make_curve(make_uniform_cubic(control_points, weights)), samples);
}

SampledCurve sample_curve(const CurveDefinition &definition, const std::span<const CurveDataPoint> data_points)
{
    SampledCurve result{.definition = definition};
    const auto curve = make_curve(definition);
    sample_positions(curve, result.samples);
    for (const auto &input : data_points)
    {
        const auto &[x, y, z] = input.position;
        const gp_Pnt position = curve.EvalD0(input.parameter);
        const double residual = std::hypot(position.X() - x, position.Y() - y, position.Z() - z);
        if (!std::isfinite(residual))
        {
            throw std::runtime_error("Point residual exceeded the finite coordinate range");
        }
        result.data_points.push_back({.position = input.position, .parameter = input.parameter, .residual = residual});
    }
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

CurveDefinition insert_curve_knot(const CurveDefinition &definition, const double parameter)
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
        throw std::invalid_argument("This knot's multiplicity already equals the curve degree");
    }
    return read_bspline_definition(curve, std::ranges::max(definition.weights));
}

CurveDefinition elevate_curve_degree(const CurveDefinition &definition, const int target_degree)
{
    if (target_degree <= definition.degree || target_degree > Geom_BSplineCurve::MaxDegree())
    {
        throw std::invalid_argument("Elevation requires a higher degree within the supported OCCT range");
    }
    auto curve = make_curve(definition);
    curve.IncreaseDegree(target_degree);
    return read_bspline_definition(curve, std::ranges::max(definition.weights));
}
} // namespace occt_lab::geometry
