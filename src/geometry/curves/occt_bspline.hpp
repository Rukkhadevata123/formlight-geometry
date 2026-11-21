#pragma once

#include "bspline_curve.hpp"

class Geom_BSplineCurve;

namespace occt_lab::geometry
{
[[nodiscard]] CurveDefinition read_bspline_definition(const Geom_BSplineCurve &curve, double weight_scale);
} // namespace occt_lab::geometry
