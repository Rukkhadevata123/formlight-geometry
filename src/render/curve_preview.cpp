#include "curve_preview.hpp"

#include "orbit_camera.hpp"

#include <imgui.h>

#include <algorithm>

namespace
{
using occt_lab::geometry::Vec3;

double clip_component(const ClipRow &row, const Vec3 &point)
{
    return row[0] * point.x + row[1] * point.y + row[2] * point.z + row[3];
}
} // namespace

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        const std::size_t selected_pole)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    constexpr double half_extent = 45.0;
    const double scale = std::max(1.0, curve.framing_extent / half_extent);
    const auto project = [&](const Vec3 &world)
    {
        const Vec3 point{.x = world.x / scale, .y = world.y / scale, .z = world.z / scale};
        const double w = clip_component(transform.w, point);
        const double x = 0.5 + 0.5 * clip_component(transform.x, point) / w;
        const double y = 0.5 + 0.5 * clip_component(transform.y, point) / w;
        return ImVec2{viewport->Pos.x + static_cast<float>(x * viewport->Size.x),
                      viewport->Pos.y + static_cast<float>(y * viewport->Size.y)};
    };
    const auto draw_polyline = [&](const std::span<const Vec3> points, const ImU32 color, const float width)
    {
        for (const auto &point : points)
        {
            draw_list.PathLineTo(project(point));
        }
        draw_list.PathStroke(color, width);
    };

    constexpr ImU32 polygon_color = IM_COL32(80, 145, 185, 225);
    constexpr ImU32 curve_color = IM_COL32(255, 185, 43, 255);
    constexpr ImU32 pole_color = IM_COL32(116, 187, 226, 255);
    const auto &poles = curve.definition.poles;
    draw_polyline(poles, polygon_color, 1.5F);
    if (curve.unit_weight_samples)
    {
        constexpr ImU32 reference_color = IM_COL32(192, 198, 204, 255);
        draw_polyline(*curve.unit_weight_samples, reference_color, 1.5F);
    }
    if (curve.approximation)
    {
        constexpr ImU32 interpolated_color = IM_COL32(206, 156, 242, 255);
        draw_polyline(curve.approximation->interpolated_samples, interpolated_color, 2.0F);
    }
    draw_polyline(curve.samples, curve_color, 2.5F);
    for (const auto &pole : poles)
    {
        draw_list.AddCircleFilled(project(pole), 4.0F, pole_color);
    }
    draw_list.AddCircle(project(poles.at(selected_pole)), 7.0F, IM_COL32(255, 255, 255, 255), 0, 1.5F);
    constexpr ImU32 data_color = IM_COL32(104, 226, 153, 255);
    for (const auto &input : curve.data_points)
    {
        const auto [x, y] = project(input.position);
        constexpr float radius = 6.0F;
        draw_list.AddQuad({x, y - radius}, {x + radius, y}, {x, y + radius}, {x - radius, y}, data_color, 2.0F);
    }
}
