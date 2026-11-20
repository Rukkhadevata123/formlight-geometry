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
    [[nodiscard]] auto patch() const -> const occt_lab::geometry::CubicBezierPatch &
    {
        return patch_;
    }
    void draw(const ClipTransform &transform, const PatchLayers &layers,
              const occt_lab::geometry::SurfaceSample *selected) const;

  private:
    occt_lab::geometry::CubicBezierPatch patch_;
    occt_lab::geometry::PatchIsolines isolines_;
};
