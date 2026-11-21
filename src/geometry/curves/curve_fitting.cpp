#include "occt_bspline.hpp"

#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Precision.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace occt_lab::geometry
{
namespace
{
std::vector<CurveDataPoint> parameterize_by_chord_length(const std::span<const Vec3> data_points)
{
    if (data_points.size() < 2 || data_points.size() > std::numeric_limits<int>::max() - 2)
    {
        throw std::invalid_argument("Curve fitting requires at least two points and an OCCT-sized array");
    }
    std::vector<CurveDataPoint> inputs;
    inputs.reserve(data_points.size());
    double length = 0.0;
    for (const auto &point : data_points)
    {
        const auto &[x, y, z] = point;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        {
            throw std::invalid_argument("Data points must have finite coordinates");
        }
        if (!inputs.empty())
        {
            const auto &[last_x, last_y, last_z] = inputs.back().position;
            const double chord = std::hypot(x - last_x, y - last_y, z - last_z);
            if (chord <= Precision::Confusion())
            {
                throw std::invalid_argument("Adjacent data points are coincident within OCCT tolerance");
            }
            length += chord;
        }
        inputs.push_back({.position = point, .parameter = length});
    }
    if (!std::isfinite(length))
    {
        throw std::invalid_argument("Total chord length exceeds the finite coordinate range");
    }
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        inputs[i].parameter /= length;
        if (i > 0 && inputs[i].parameter <= inputs[i - 1].parameter)
        {
            throw std::invalid_argument("Chord lengths produce indistinguishable chord parameters");
        }
    }
    return inputs;
}

// Both solvers receive the same parameters, and residuals are measured at those parameters.
SampledCurve fit_curve(const std::span<const Vec3> data_points, const std::optional<double> tolerance)
{
    const auto inputs = parameterize_by_chord_length(data_points);
    const int count = static_cast<int>(inputs.size());
    const occ::handle points{new NCollection_HArray1<gp_Pnt>(1, count)};
    const occ::handle parameters{new NCollection_HArray1<double>(1, count)};
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        const int index = static_cast<int>(i) + 1;
        const auto &[x, y, z] = inputs[i].position;
        points->SetValue(index, gp_Pnt{x, y, z});
        parameters->SetValue(index, inputs[i].parameter);
    }

    GeomAPI_Interpolate interpolation{points, parameters, false, Precision::Confusion()};
    interpolation.Perform();
    if (!interpolation.IsDone())
    {
        throw std::runtime_error("OCCT could not interpolate these points");
    }
    auto interpolated = sample_curve(read_bspline_definition(*interpolation.Curve(), 1.0), inputs);
    if (!tolerance)
    {
        return interpolated;
    }

    const int degree = std::min(3, count - 1);
    constexpr std::array continuity_by_degree{GeomAbs_C0, GeomAbs_C1, GeomAbs_C2};
    const auto continuity = continuity_by_degree[degree - 1];
    GeomAPI_PointsToBSpline solver{points->Array1(), parameters->Array1(), degree, degree, continuity, *tolerance};
    if (!solver.IsDone())
    {
        throw std::runtime_error("OCCT could not approximate these points with the requested tolerance");
    }
    auto fitted = sample_curve(read_bspline_definition(*solver.Curve(), 1.0), inputs);
    fitted.approximation = compare_to_interpolant(interpolated, *tolerance);
    fitted.framing_extent = std::max(fitted.framing_extent, interpolated.framing_extent);
    return fitted;
}
} // namespace

SampledCurve::ApproximationComparison compare_to_interpolant(const SampledCurve &interpolated,
                                                             const double requested_tolerance)
{
    SampledCurve::ApproximationComparison comparison{.requested_tolerance = requested_tolerance,
                                                     .interpolated_samples = interpolated.samples,
                                                     .interpolated_residuals = {}};
    for (const auto &input : interpolated.data_points)
    {
        comparison.interpolated_residuals.push_back(input.residual);
    }
    return comparison;
}

SampledCurve interpolate_curve(const std::span<const Vec3> data_points)
{
    return fit_curve(data_points, std::nullopt);
}

SampledCurve approximate_curve(const std::span<const Vec3> data_points, const double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance <= 0.0)
    {
        throw std::invalid_argument("Approximation tolerance must be finite and greater than zero");
    }
    return fit_curve(data_points, tolerance);
}

} // namespace occt_lab::geometry
