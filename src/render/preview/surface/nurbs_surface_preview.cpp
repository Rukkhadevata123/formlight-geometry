#include "nurbs_surface_preview.hpp"

#include "surface_probe_marker.hpp"
#include "surface_wireframe.hpp"

occt_lab::render::ViewportProjection nurbs_surface_projection(const ClipTransform &transform,
                                                              const EditableNurbsSurface::Framing &frame)
{
    // Frame the longest side in the same 90-unit view as the Bezier example.
    const double scale = frame.span > 0.0 ? frame.span / 90.0 : 1.0;
    return {transform, *ImGui::GetMainViewport(), frame.center, scale};
}

void draw_nurbs_surface_preview(const EditableNurbsSurface::Snapshot &surface, const ClipTransform &transform,
                                const PatchLayers &layers, const std::size_t selected_pole,
                                const occt_lab::geometry::SurfaceProbe *probe)
{
    const auto project = nurbs_surface_projection(transform, surface.framing);
    auto &draw_list = *ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    draw_surface_wireframe(draw_list, project, surface.definition.poles, surface.definition.u.pole_count,
                           surface.isolines, layers, selected_pole);
    if (probe)
    {
        const auto normal = probe->normal ? std::optional{*probe->normal} : std::nullopt;
        draw_surface_probe_marker(draw_list, project, probe->position, normal, surface.framing.span * (12.0 / 90.0));
    }
}
