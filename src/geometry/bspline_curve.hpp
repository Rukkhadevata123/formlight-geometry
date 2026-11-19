#pragma once

#include "vec3.hpp"

#include <array>
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
    static constexpr int degree = 3;
    std::vector<Vec3> poles;
    std::vector<double> weights;
    std::vector<CurveKnot> knots;

    bool operator==(const CurveDefinition &) const = default;
};

struct SampledCurve
{
    using Samples = std::array<Vec3, 129>;
    CurveDefinition definition;
    Samples samples{};
    std::optional<Samples> unit_weight_samples;
    double framing_extent = 0.0;
    std::optional<double> insertion_sample_error;
};

[[nodiscard]] CurveDefinition make_uniform_cubic(std::span<const Vec3> poles, std::span<const double> weights = {});
[[nodiscard]] SampledCurve sample_curve(const CurveDefinition &definition);
[[nodiscard]] CurveDefinition insert_cubic_knot(const CurveDefinition &definition, double parameter);

// Samples a non-periodic cubic B-spline with uniformly spaced interior knots, including both endpoints.
// An empty weight span assigns unit weight to every pole.
void sample_clamped_cubic_bspline(std::span<const Vec3> control_points, std::span<Vec3> samples,
                                  std::span<const double> weights = {});
} // namespace occt_lab::geometry
