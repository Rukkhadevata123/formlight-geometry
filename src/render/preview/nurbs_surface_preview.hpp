#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "render/viewport/viewport_projection.hpp"

struct ClipTransform;
struct PatchLayers;

class NurbsSurfacePreview
{
  public:
    explicit NurbsSurfacePreview(std::span<const occt_lab::geometry::Vec3> initial_poles);
    [[nodiscard]] std::optional<std::size_t> pick_pole(std::span<const occt_lab::geometry::Vec3> poles,
                                                       const ClipTransform &transform, ImVec2 cursor) const;
    void draw(const EditableNurbsSurface::Snapshot &surface, const ClipTransform &transform, const PatchLayers &layers,
              std::size_t selected_pole) const;

  private:
    [[nodiscard]] occt_lab::render::ViewportProjection projection(const ClipTransform &transform) const;
    occt_lab::geometry::Vec3 center_;
    double scale_;
};
