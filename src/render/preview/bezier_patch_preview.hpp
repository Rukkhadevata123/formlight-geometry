#pragma once

#include "geometry/surfaces/bezier_patch.hpp"

#include <optional>

struct ClipTransform;
struct ImVec2;
class EditablePatch;
struct PatchLayers;

[[nodiscard]] std::optional<std::size_t> pick_patch_pole(const EditablePatch &model, const ClipTransform &transform,
                                                         ImVec2 cursor);
[[nodiscard]] occt_lab::geometry::Vec3 dragged_patch_pole_position(const EditablePatch &model,
                                                                   const ClipTransform &transform, std::size_t index,
                                                                   ImVec2 screen_delta);
void draw_bezier_patch_preview(const EditablePatch &model, const ClipTransform &transform, const PatchLayers &layers,
                               std::size_t selected_pole, const occt_lab::geometry::SurfaceSample *selected);
