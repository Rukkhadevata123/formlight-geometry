#pragma once

#include "vec3.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace occt_lab::geometry
{
struct CurveDefinition
{
    static constexpr std::size_t pole_count = 6;
    using Poles = std::array<Vec3, pole_count>;
    Poles poles{};
    std::array<double, pole_count> weights{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

    bool operator==(const CurveDefinition &) const = default;
};

struct SampledCurve
{
    using Samples = std::array<Vec3, 129>;
    CurveDefinition definition;
    Samples samples{};
    std::optional<Samples> unit_weight_samples;
};

// Samples a non-periodic cubic B-spline with uniformly spaced interior knots, including both endpoints.
// An empty weight span assigns unit weight to every pole.
void sample_clamped_cubic_bspline(std::span<const Vec3> control_points, std::span<Vec3> samples,
                                  std::span<const double> weights = {});
} // namespace occt_lab::geometry
