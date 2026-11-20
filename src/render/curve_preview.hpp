#pragma once

#include "geometry/vec3.hpp"

#include <cstddef>
#include <optional>

namespace occt_lab::geometry
{
struct SampledCurve;
}

struct ClipTransform;
struct ImVec2;

[[nodiscard]] auto pick_curve_pole(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                                   ImVec2 cursor) -> std::optional<std::size_t>;
[[nodiscard]] auto dragged_pole_position(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                                         std::size_t index, ImVec2 screen_delta) -> occt_lab::geometry::Vec3;
void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        std::size_t selected_pole);
