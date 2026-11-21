#pragma once

namespace occt_lab::geometry
{
struct BSplineKnot
{
    double value;
    int multiplicity;
    bool operator==(const BSplineKnot &) const = default;
};
} // namespace occt_lab::geometry
