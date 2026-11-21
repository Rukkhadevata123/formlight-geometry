#pragma once

#include "bspline_knot.hpp"

#include <NCollection_Array1.hxx>

#include <span>

namespace occt_lab::geometry
{
struct OcctKnotArrays
{
    NCollection_Array1<double> values;
    NCollection_Array1<int> multiplicities;
};

// The caller has checked the degree and pole count against the OCCT limits.
[[nodiscard]] OcctKnotArrays make_clamped_knots(std::span<const BSplineKnot> knots, int degree, int pole_count);
} // namespace occt_lab::geometry
