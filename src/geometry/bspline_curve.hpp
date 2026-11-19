#pragma once

#include "vec3.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace occt_lab::geometry
{
struct CurveKnot
{
    double value;
    int multiplicity;
    bool operator==(const CurveKnot &) const = default;
};

struct CurveDefinition
{
    int degree = 3;
    std::vector<Vec3> poles;
    std::vector<double> weights;
    std::vector<CurveKnot> knots;

    bool operator==(const CurveDefinition &) const = default;
};

struct RefinementComparison
{
    int previous_degree;
    std::size_t previous_pole_count;
    double max_sample_deviation;
    bool operator==(const RefinementComparison &) const = default;
};

struct SampledCurve
{
    using Samples = std::array<Vec3, 129>;
    CurveDefinition definition;
    Samples samples{};
    std::optional<Samples> unit_weight_samples;
    double framing_extent = 0.0;
    std::optional<RefinementComparison> refinement;
};

[[nodiscard]] CurveDefinition make_uniform_cubic(std::span<const Vec3> poles, std::span<const double> weights = {});
[[nodiscard]] SampledCurve sample_curve(const CurveDefinition &definition);
[[nodiscard]] CurveDefinition insert_curve_knot(const CurveDefinition &definition, double parameter);
[[nodiscard]] CurveDefinition elevate_curve_degree(const CurveDefinition &definition, int target_degree);

// Samples a non-periodic cubic B-spline with uniformly spaced interior knots, including both endpoints.
// An empty weight span assigns unit weight to every pole.
void sample_clamped_cubic_bspline(std::span<const Vec3> control_points, std::span<Vec3> samples,
                                  std::span<const double> weights = {});
} // namespace occt_lab::geometry
