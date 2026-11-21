#pragma once

#include "geometry/bezier_patch.hpp"

struct ClipTransform;
class EditablePatch;

struct PatchLayers
{
    bool control_net = true;
    bool constant_u = true;
    bool constant_v = true;
};

void draw_bezier_patch_preview(const EditablePatch &model, const ClipTransform &transform, const PatchLayers &layers,
                               const occt_lab::geometry::SurfaceSample *selected);
