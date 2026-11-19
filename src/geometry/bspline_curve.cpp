#include "bspline_curve.hpp"

#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
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
} // namespace

void sample_clamped_cubic_bspline(const std::span<const Vec3> control_points, const std::span<Vec3> samples,
                                  const std::span<const double> weights)
{
    constexpr int degree = 3;
    if (control_points.size() < degree + 1 || control_points.size() > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("A cubic B-spline requires at least four poles and an OCCT-sized array");
    }
    if (samples.size() < 2)
    {
        throw std::invalid_argument("B-spline sampling requires space for both endpoints");
    }
    const bool finite_poles =
        std::ranges::all_of(control_points, [](const Vec3 &point)
                            { return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z); });
    if (!finite_poles)
    {
        throw std::invalid_argument("B-spline poles must have finite coordinates");
    }

    const auto pole_count = static_cast<int>(control_points.size());
    NCollection_Array1<gp_Pnt> poles{1, pole_count};
    for (std::size_t index = 0; index < control_points.size(); ++index)
    {
        const auto &[x, y, z] = control_points[index];
        poles(static_cast<int>(index) + 1) = gp_Pnt{x, y, z};
    }

    // OCCT stores distinct knots and multiplicities. Clamping repeats each end knot degree + 1 times.
    const int knot_count = pole_count - degree + 1;
    NCollection_Array1<double> knots{1, knot_count};
    NCollection_Array1<int> multiplicities{1, knot_count};
    for (int index = 1; index <= knot_count; ++index)
    {
        knots(index) = static_cast<double>(index - 1) / (knot_count - 1);
        const bool at_endpoint = index == 1 || index == knot_count;
        multiplicities(index) = at_endpoint ? degree + 1 : 1;
    }

    constexpr bool periodic = false;
    const Geom_BSplineCurve curve = weights.empty() ? Geom_BSplineCurve{poles, knots, multiplicities, degree, periodic}
                                                    : Geom_BSplineCurve{poles,  normalized_weights(weights, pole_count),
                                                                        knots,  multiplicities,
                                                                        degree, periodic};
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
} // namespace occt_lab::geometry
