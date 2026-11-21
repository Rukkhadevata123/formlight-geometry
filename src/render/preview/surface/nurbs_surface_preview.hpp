#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "render/viewport/viewport_projection.hpp"

struct ClipTransform;
struct PatchLayers;

[[nodiscard]] occt_lab::render::ViewportProjection nurbs_surface_projection(const ClipTransform &transform,
                                                                            const EditableNurbsSurface::Framing &frame);
void draw_nurbs_surface_preview(const EditableNurbsSurface::Snapshot &surface, const ClipTransform &transform,
                                const PatchLayers &layers, std::size_t selected_pole,
                                const occt_lab::geometry::SurfaceProbe *probe);
