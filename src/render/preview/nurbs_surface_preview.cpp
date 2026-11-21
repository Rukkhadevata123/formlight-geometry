#include "nurbs_surface_preview.hpp"

#include "surface_wireframe.hpp"

#include <Bnd_Box.hxx>

#include <algorithm>
#include <numeric>

NurbsSurfacePreview::NurbsSurfacePreview(const std::span<const occt_lab::geometry::Vec3> initial_poles)
{
    Bnd_Box bounds;
    for (const auto &[x, y, z] : initial_poles)
    {
        bounds.Add(gp_Pnt{x, y, z});
    }
    const auto low = bounds.CornerMin();
    const auto high = bounds.CornerMax();
    center_ = {std::midpoint(low.X(), high.X()), std::midpoint(low.Y(), high.Y()), std::midpoint(low.Z(), high.Z())};
    const double span = std::max({high.X() - low.X(), high.Y() - low.Y(), high.Z() - low.Z()});
    // Frame the longest side in the same 90-unit view as the Bezier example.
    scale_ = span > 0.0 ? span / 90.0 : 1.0;
}

occt_lab::render::ViewportProjection NurbsSurfacePreview::projection(const ClipTransform &transform) const
{
    return {transform, *ImGui::GetMainViewport(), center_, scale_};
}

std::optional<std::size_t> NurbsSurfacePreview::pick_pole(const std::span<const occt_lab::geometry::Vec3> poles,
                                                          const ClipTransform &transform, const ImVec2 cursor) const
{
    return projection(transform).pick_pole(poles, cursor);
}

void NurbsSurfacePreview::draw(const EditableNurbsSurface::Snapshot &surface, const ClipTransform &transform,
                               const PatchLayers &layers, const std::size_t selected_pole) const
{
    draw_surface_wireframe(*ImGui::GetBackgroundDrawList(ImGui::GetMainViewport()), projection(transform),
                           surface.definition.poles, surface.definition.u.pole_count, surface.isolines, layers,
                           selected_pole);
}
