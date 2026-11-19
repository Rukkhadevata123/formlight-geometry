#include "curve_preview.hpp"

#include "orbit_camera.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace
{
using occt_lab::geometry::Vec3;

double clip_component(const ClipRow &row, const Vec3 &point)
{
    return row[0] * point.x + row[1] * point.y + row[2] * point.z + row[3];
}

template <std::size_t Count>
std::array<ImVec2, Count> project_points(const std::array<Vec3, Count> &points, const ClipTransform &transform,
                                         const ImGuiViewport &viewport, const double scale)
{
    std::array<ImVec2, Count> projected;
    std::ranges::transform(points, projected.begin(),
                           [&transform, &viewport, scale](const Vec3 &world)
                           {
                               const Vec3 point{.x = world.x / scale, .y = world.y / scale, .z = world.z / scale};
                               const double w = clip_component(transform.w, point);
                               const double x = 0.5 + 0.5 * clip_component(transform.x, point) / w;
                               const double y = 0.5 + 0.5 * clip_component(transform.y, point) / w;
                               return ImVec2{
                                   viewport.Pos.x + static_cast<float>(x * viewport.Size.x),
                                   viewport.Pos.y + static_cast<float>(y * viewport.Size.y),
                               };
                           });
    return projected;
}
} // namespace

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        const std::size_t selected_pole)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    // Bound the entire control hull before perspective division, including at the closest zoom.
    double scale = 1.0;
    const auto &poles = curve.definition.poles;
    for (const auto &[x, y, z] : poles)
    {
        constexpr double half_extent = 45.0;
        scale = std::max(scale, std::max({std::abs(x), std::abs(y), std::abs(z)}) / half_extent);
    }
    const auto projected_controls = project_points(poles, transform, *viewport, scale);
    const auto projected_samples = project_points(curve.samples, transform, *viewport, scale);

    constexpr ImU32 polygon_color = IM_COL32(80, 145, 185, 225);
    constexpr ImU32 curve_color = IM_COL32(255, 185, 43, 255);
    constexpr ImU32 pole_color = IM_COL32(116, 187, 226, 255);
    draw_list.AddPolyline(projected_controls.data(), static_cast<int>(projected_controls.size()), polygon_color, 1.5F);
    if (curve.unit_weight_samples)
    {
        const auto reference = project_points(*curve.unit_weight_samples, transform, *viewport, scale);
        constexpr ImU32 reference_color = IM_COL32(192, 198, 204, 255);
        draw_list.AddPolyline(reference.data(), static_cast<int>(reference.size()), reference_color, 1.5F);
    }
    draw_list.AddPolyline(projected_samples.data(), static_cast<int>(projected_samples.size()), curve_color, 2.5F);
    for (const auto &position : projected_controls)
    {
        draw_list.AddCircleFilled(position, 4.0F, pole_color);
    }
    draw_list.AddCircle(projected_controls.at(selected_pole), 7.0F, IM_COL32(255, 255, 255, 255), 0, 1.5F);
}
