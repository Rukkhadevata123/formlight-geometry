#include "bezier_patch_preview.hpp"

#include "application/editable_patch.hpp"
#include "surface_probe_marker.hpp"
#include "surface_wireframe.hpp"

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::Vec3;
constexpr auto side = CubicBezierPatch::side_length;
constexpr double half_width = 45.0;

} // namespace

std::optional<std::size_t> pick_patch_pole(const EditablePatch &model, const ClipTransform &transform,
                                           const ImVec2 cursor)
{
    const occt_lab::render::ViewportProjection project{transform, *ImGui::GetMainViewport(), half_width};
    return project.pick_pole(model.current().control_net(), cursor);
}

Vec3 dragged_patch_pole_position(const EditablePatch &model, const ClipTransform &transform, const std::size_t index,
                                 const ImVec2 screen_delta)
{
    const occt_lab::render::ViewportProjection project{transform, *ImGui::GetMainViewport(), half_width};
    return project.dragged_position(model.current().control_net().at(index), screen_delta);
}

void draw_bezier_patch_preview(const EditablePatch &model, const ClipTransform &transform, const PatchLayers &layers,
                               const std::size_t selected_pole, const occt_lab::geometry::SurfaceSample *selected)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const occt_lab::render::ViewportProjection project{transform, *viewport, half_width};
    const auto &net = model.displayed().control_net();
    draw_surface_wireframe(draw_list, project, net, side, model.isolines(), layers, selected_pole);
    if (selected)
    {
        draw_surface_probe_marker(draw_list, project, selected->position, selected->normal, 12.0);
    }
}
