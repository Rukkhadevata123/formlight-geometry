#include "curve_preview.hpp"

#include "curve_colors.hpp"
#include "geometry/curves/bspline_curve.hpp"
#include "render/viewport/viewport_projection.hpp"

using occt_lab::geometry::Vec3;
using occt_lab::render::ViewportProjection;

auto pick_curve_pole(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform, const ImVec2 cursor)
    -> std::optional<std::size_t>
{
    const auto &viewport = *ImGui::GetMainViewport();
    const ViewportProjection project{transform, viewport, curve.framing_extent};
    return project.pick_pole(curve.definition.poles, cursor);
}

auto dragged_pole_position(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                           const std::size_t index, const ImVec2 screen_delta) -> Vec3
{
    const auto &viewport = *ImGui::GetMainViewport();
    const ViewportProjection project{transform, viewport, curve.framing_extent};
    return project.dragged_position(curve.definition.poles.at(index), screen_delta);
}

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        const std::optional<std::size_t> selected_pole)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const ViewportProjection project{transform, *viewport, curve.framing_extent};

    constexpr ImU32 polygon_color = IM_COL32(80, 145, 185, 225);
    constexpr ImU32 pole_color = IM_COL32(116, 187, 226, 255);
    const auto &poles = curve.definition.poles;
    project.draw_polyline(draw_list, poles, polygon_color, 1.5F);
    if (curve.unit_weight_samples)
    {
        constexpr ImU32 reference_color = IM_COL32(192, 198, 204, 255);
        project.draw_polyline(draw_list, *curve.unit_weight_samples, reference_color, 1.5F);
    }
    if (curve.approximation)
    {
        project.draw_polyline(draw_list, curve.approximation->interpolated_samples, curve_colors::interpolant_reference,
                              2.0F);
    }
    project.draw_polyline(draw_list, curve.samples, curve_colors::current, 2.5F);
    for (const auto &pole : poles)
    {
        if (const auto point = project.visible_point(pole))
        {
            draw_list.AddCircleFilled(*point, 4.0F, pole_color);
        }
    }
    if (selected_pole)
    {
        if (const auto selected = project.visible_point(poles.at(*selected_pole)))
        {
            draw_list.AddCircle(*selected, 7.0F, IM_COL32(255, 255, 255, 255), 0, 1.5F);
        }
    }
    constexpr ImU32 data_color = IM_COL32(104, 226, 153, 255);
    for (const auto &input : curve.data_points)
    {
        const auto point = project.visible_point(input.position);
        if (!point)
        {
            continue;
        }
        const auto [x, y] = *point;
        constexpr float radius = 6.0F;
        draw_list.AddQuad({x, y - radius}, {x + radius, y}, {x, y + radius}, {x - radius, y}, data_color, 2.0F);
    }
}
