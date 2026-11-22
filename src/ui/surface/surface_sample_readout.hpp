#pragma once

#include "geometry/surfaces/bezier_patch.hpp"

inline constexpr char parameter_slider_hint[] = "Ctrl+click a slider to type a value, then press Enter.";

void draw_surface_sample(const occt_lab::geometry::SurfaceSample &sample);
void draw_bernstein_sample(
    const std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> &sample);
void draw_surface_difference(const occt_lab::geometry::SurfaceSample &sample,
                             const occt_lab::geometry::SurfaceSample &reference);
