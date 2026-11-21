#pragma once

#include "geometry/surfaces/bspline_surface.hpp"

struct ClipTransform;
struct PatchLayers;

class NurbsSurfacePreview
{
  public:
    NurbsSurfacePreview();
    void draw(const ClipTransform &transform, const PatchLayers &layers) const;

  private:
    occt_lab::geometry::BSplineSurfaceDefinition definition_;
    occt_lab::geometry::PatchIsolines isolines_;
    occt_lab::geometry::Vec3 center_;
    double scale_;
};
