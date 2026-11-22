#pragma once

#include "geometry/bspline_knot.hpp"
#include "geometry/vec3.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace occt_lab::geometry
{
struct CurveDefinition
{
    int degree = 3;
    std::vector<Vec3> poles;
    std::vector<double> weights;
    std::vector<BSplineKnot> knots;

    bool operator==(const CurveDefinition &) const = default;
};

struct RefinementComparison
{
    int previous_degree;
    std::size_t previous_pole_count;
    double max_sample_deviation;
    bool operator==(const RefinementComparison &) const = default;
};

struct CurveDataPoint
{
    Vec3 position{};
    double parameter = 0.0;
    double residual = 0.0;
    bool operator==(const CurveDataPoint &) const = default;
};

struct SampledCurve
{
    using Samples = std::array<Vec3, 129>;
    struct ApproximationComparison
    {
        double requested_tolerance = 0.0;
        Samples interpolated_samples{};
        std::vector<double> interpolated_residuals;
        bool operator==(const ApproximationComparison &) const = default;
    };
    CurveDefinition definition;
    Samples samples{};
    std::optional<Samples> unit_weight_samples;
    double framing_extent = 0.0;
    std::optional<RefinementComparison> refinement;
    std::vector<CurveDataPoint> data_points;
    std::optional<ApproximationComparison> approximation;
    bool operator==(const SampledCurve &) const = default;
};

[[nodiscard]] CurveDefinition make_uniform_cubic(std::span<const Vec3> poles, std::span<const double> weights = {});
[[nodiscard]] SampledCurve sample_curve(const CurveDefinition &definition,
                                        std::span<const CurveDataPoint> data_points = {});
[[nodiscard]] SampledCurve::ApproximationComparison compare_to_interpolant(const SampledCurve &interpolated,
                                                                           double requested_tolerance);

// clang-format off
[[nodiscard]] auto interpolate_curve(std::span<const Vec3> data_points)                       -> SampledCurve;
[[nodiscard]] auto approximate_curve(std::span<const Vec3> data_points, double tolerance)     -> SampledCurve;
[[nodiscard]] auto insert_curve_knot(const CurveDefinition &definition, double parameter)     -> CurveDefinition;
[[nodiscard]] auto elevate_curve_degree(const CurveDefinition &definition, int target_degree) -> CurveDefinition;
// clang-format on

} // namespace occt_lab::geometry
