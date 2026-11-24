module;

#include <imgui.h>

export module occt_lab.render.preview.surface.nurbs_surface_preview;

import std;
import occt_lab.application.editable_nurbs_surface;
import occt_lab.geometry.surfaces.bspline_surface;
import occt_lab.render.preview.surface.surface_probe_marker;
import occt_lab.render.preview.surface.surface_wireframe;
import occt_lab.render.viewport.clip_transform;
import occt_lab.render.viewport.viewport_projection;

export [[nodiscard]] occt_lab::render::ViewportProjection nurbs_surface_projection(
    const ClipTransform &transform, const EditableNurbsSurface::Framing &frame)
{
    // Frame the longest side in the same 90-unit view as the Bezier example.
    return {transform, *ImGui::GetMainViewport(), frame.center, frame.world_units_per_view_unit()};
}

export void draw_nurbs_surface_preview(const EditableNurbsSurface::Snapshot &surface, const ClipTransform &transform,
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
