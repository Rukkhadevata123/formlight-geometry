#pragma once

#include "geometry/bezier_patch.hpp"

struct ClipTransform;

struct PatchLayers
{
    bool control_net = true;
    bool constant_u = true;
    bool constant_v = true;
};

class BezierPatchPreview
{
  public:
    BezierPatchPreview();
    void draw(const ClipTransform &transform, const PatchLayers &layers) const;

  private:
    occt_lab::geometry::CubicBezierPatch::ControlNet control_net_;
    occt_lab::geometry::PatchIsolines isolines_;
};
