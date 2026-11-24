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
                                       const PatchLayers &layers, const occt_lab::geometry::Vec3 &selected_point,
                                       const occt_lab::geometry::SurfaceProbe *probe)
{
    const auto project = nurbs_surface_projection(transform, surface.framing);
    auto &draw_list = *ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    draw_surface_wireframe(draw_list, project, surface.definition.poles, surface.definition.u.pole_count,
                           surface.isolines, layers, selected_point);
    if (const auto &grid = surface.definition.data_grid)
    {
        for (const auto &point : grid->points)
        {
            if (const auto pixel = project.visible_point(point))
            {
                draw_list.AddCircle(*pixel, 5.0F, IM_COL32(233, 140, 235, 255), 0, 1.5F);
            }
        }
    }
    if (const auto &deviation = surface.max_grid_deviation)
    {
        constexpr ImU32 highlight = IM_COL32(255, 78, 184, 255);
        project.draw_polyline(draw_list, std::array{deviation->data_point, deviation->surface_point}, highlight, 2.0F);
        if (const auto data = project.visible_point(deviation->data_point))
        {
            draw_list.AddCircle(*data, 9.0F, highlight, 0, 2.5F);
        }
        if (const auto fitted = project.visible_point(deviation->surface_point))
        {
            draw_list.AddCircleFilled(*fitted, 3.0F, highlight);
        }
    }
    if (probe)
    {
        const auto normal = probe->normal ? std::optional{*probe->normal} : std::nullopt;
        draw_surface_probe_marker(draw_list, project, probe->position, normal, surface.framing.span * (12.0 / 90.0));
    }
}
