#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "render/viewport/viewport_projection.hpp"

struct ClipTransform;
struct PatchLayers;

class NurbsSurfacePreview
{
  public:
    explicit NurbsSurfacePreview(std::span<const occt_lab::geometry::Vec3> initial_poles);
    [[nodiscard]] occt_lab::render::ViewportProjection projection(const ClipTransform &transform) const;
    static void draw(const EditableNurbsSurface::Snapshot &surface, const occt_lab::render::ViewportProjection &project,
                     const PatchLayers &layers, std::size_t selected_pole);

  private:
    occt_lab::geometry::Vec3 center_;
    double scale_;
};
