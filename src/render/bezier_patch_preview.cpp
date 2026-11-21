#include "bezier_patch_preview.hpp"

#include "application/editable_patch.hpp"
#include "viewport_projection.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::Vec3;
constexpr auto side = CubicBezierPatch::side_length;
constexpr double half_width = 45.0;

void draw_surface_normal(ImDrawList &draw_list, const occt_lab::render::ViewportProjection &project,
                         const occt_lab::geometry::SurfaceSample &sample)
{
    const auto &[point, normal] = sample;
    const auto base = project.visible_point(point);
    if (!base)
    {
        return;
    }
    constexpr double arrow_length_mm = 12.0;
    const Vec3 tip{.x = point.x + arrow_length_mm * normal.x,
                   .y = point.y + arrow_length_mm * normal.y,
                   .z = point.z + arrow_length_mm * normal.z};
    constexpr ImU32 color = IM_COL32(255, 150, 118, 255);
    project.draw_polyline(draw_list, std::array{point, tip}, color, 2.5F);
    if (const auto end = project.visible_point(tip))
    {
        const float dx = end->x - base->x;
        const float dy = end->y - base->y;
        if (const float length = std::hypot(dx, dy); length > 1.0F)
        {
            // Size the arrowhead in screen space and shrink it when the normal points toward the eye.
            const float scale = std::min(8.0F / length, 0.4F);
            const ImVec2 back{end->x - scale * dx, end->y - scale * dy};
            const ImVec2 wing{-0.5F * scale * dy, 0.5F * scale * dx};
            draw_list.AddTriangleFilled(*end, {back.x + wing.x, back.y + wing.y}, {back.x - wing.x, back.y - wing.y},
                                        color);
        }
    }
    draw_list.AddCircleFilled(*base, 4.0F, color);
    draw_list.AddCircle(*base, 6.0F, IM_COL32_WHITE, 0, 1.5F);
}
} // namespace

void draw_bezier_patch_preview(const EditablePatch &model, const ClipTransform &transform, const PatchLayers &layers,
                               const std::size_t selected_pole, const occt_lab::geometry::SurfaceSample *selected)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const occt_lab::render::ViewportProjection project{transform, *viewport, half_width};
    constexpr ImU32 net_color = IM_COL32(111, 178, 216, 210);
    if (layers.control_net)
    {
        const std::span net{model.current().control_net()};
        for (std::size_t i = 0; i < side; ++i)
        {
            project.draw_polyline(draw_list, net.subspan(i * side, side), net_color, 1.25F);
            std::array<Vec3, side> column{};
            for (std::size_t row = 0; row < side; ++row)
            {
                column[row] = net[row * side + i];
            }
            project.draw_polyline(draw_list, column, net_color, 1.25F);
        }
    }
    if (layers.constant_u)
    {
        for (const auto &line : model.isolines().constant_u)
        {
            project.draw_polyline(draw_list, line, IM_COL32(255, 185, 43, 255), 2.0F);
        }
    }
    if (layers.constant_v)
    {
        for (const auto &line : model.isolines().constant_v)
        {
            project.draw_polyline(draw_list, line, IM_COL32(104, 226, 153, 255), 2.0F);
        }
    }
    if (layers.control_net)
    {
        for (const auto &pole : model.current().control_net())
        {
            if (const auto point = project.visible_point(pole))
            {
                draw_list.AddCircleFilled(*point, 4.0F, net_color);
            }
        }
        if (const auto point = project.visible_point(model.current().control_net().at(selected_pole)))
        {
            draw_list.AddCircle(*point, 7.0F, IM_COL32_WHITE, 0, 1.5F);
        }
    }
    if (selected)
    {
        draw_surface_normal(draw_list, project, *selected);
    }
}
