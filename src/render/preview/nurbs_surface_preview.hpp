#pragma once

#include "geometry/surfaces/bspline_surface.hpp"
#include "render/viewport/viewport_projection.hpp"

struct ClipTransform;
struct PatchLayers;

class NurbsSurfacePreview
{
  public:
    NurbsSurfacePreview();
    [[nodiscard]] const occt_lab::geometry::BSplineSurfaceDefinition &definition() const
    {
        return definition_;
    }
    [[nodiscard]] std::optional<std::size_t> pick_pole(const ClipTransform &transform, ImVec2 cursor) const;
    void draw(const ClipTransform &transform, const PatchLayers &layers, std::size_t selected_pole) const;

  private:
    [[nodiscard]] occt_lab::render::ViewportProjection projection(const ClipTransform &transform) const;
    occt_lab::geometry::BSplineSurfaceDefinition definition_;
    occt_lab::geometry::PatchIsolines isolines_;
    occt_lab::geometry::Vec3 center_;
    double scale_;
};
