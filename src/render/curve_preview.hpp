#pragma once

#include "geometry/bspline_curve.hpp"

#include <cstddef>

struct ClipTransform;

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        std::size_t selected_pole);
