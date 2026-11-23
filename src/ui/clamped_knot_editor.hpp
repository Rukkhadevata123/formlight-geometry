#pragma once

#include "geometry/bspline_knot.hpp"

#include <optional>

// Returns true when the custom table or the choice of knot mode changes.
bool draw_custom_knots(const char *label, int poles, int degree,
                       std::optional<std::vector<occt_lab::geometry::BSplineKnot>> &knots);
