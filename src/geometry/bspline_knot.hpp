#pragma once

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
} // namespace occt_lab::geometry
