#pragma once

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
void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        std::size_t selected_pole);
