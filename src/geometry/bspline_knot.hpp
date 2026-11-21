#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace occt_lab::geometry
{
struct BSplineKnot
{
    double value;
    int multiplicity;
    bool operator==(const BSplineKnot &) const = default;
};

[[nodiscard]] std::vector<BSplineKnot> make_uniform_clamped_knots(int pole_count, int degree);
[[nodiscard]] auto validate_clamped_knots(std::span<const BSplineKnot> knots, int degree, int pole_count)
    -> std::expected<void, std::string>;
} // namespace occt_lab::geometry
