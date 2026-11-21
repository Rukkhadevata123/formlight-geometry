#pragma once

#include "geometry/bezier_patch.hpp"

#include <optional>

struct ClipTransform;
struct ImVec2;
class EditablePatch;

struct PatchLayers
{
    bool control_net = true;
    bool constant_u = true;
    bool constant_v = true;
};

[[nodiscard]] std::optional<std::size_t> pick_patch_pole(const EditablePatch &model, const ClipTransform &transform,
                                                         ImVec2 cursor);
void draw_bezier_patch_preview(const EditablePatch &model, const ClipTransform &transform, const PatchLayers &layers,
                               std::size_t selected_pole, const occt_lab::geometry::SurfaceSample *selected);
