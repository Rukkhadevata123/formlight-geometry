#include "curve_preview.hpp"

#include "geometry/bspline_curve.hpp"
#include "orbit_camera.hpp"

#include <imgui.h>

#include <algorithm>

namespace
{
using occt_lab::geometry::Vec3;

// The control hull stays inside the camera's depth range throughout its orbit.
constexpr std::array control_points{
    Vec3{.x = 42.0, .y = -42.0, .z = 0.0}, Vec3{.x = 5.0, .y = -38.0, .z = 0.0}, Vec3{.x = -10.0, .y = -26.0, .z = 0.0},
    Vec3{.x = 10.0, .y = 26.0, .z = 0.0},  Vec3{.x = -5.0, .y = 38.0, .z = 0.0}, Vec3{.x = -42.0, .y = 42.0, .z = 0.0},
};

double clip_component(const ClipRow &row, const Vec3 &point)
{
    return row[0] * point.x + row[1] * point.y + row[2] * point.z + row[3];
}

template <std::size_t Count>
std::array<ImVec2, Count> project_points(const std::array<Vec3, Count> &points, const ClipTransform &transform,
                                         const ImGuiViewport &viewport)
{
    std::array<ImVec2, Count> projected;
    std::ranges::transform(points, projected.begin(),
                           [&transform, &viewport](const Vec3 &point)
                           {
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

CurvePreview::CurvePreview()
{
    occt_lab::geometry::sample_clamped_cubic_bspline(control_points, samples_);
}

void CurvePreview::draw(const ClipTransform &transform) const
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const auto projected_controls = project_points(control_points, transform, *viewport);
    const auto projected_samples = project_points(samples_, transform, *viewport);

    constexpr ImU32 polygon_color = IM_COL32(80, 145, 185, 225);
    constexpr ImU32 curve_color = IM_COL32(255, 185, 43, 255);
    constexpr ImU32 pole_color = IM_COL32(116, 187, 226, 255);
    draw_list.AddPolyline(projected_controls.data(), static_cast<int>(projected_controls.size()), polygon_color, 1.5F);
    draw_list.AddPolyline(projected_samples.data(), static_cast<int>(projected_samples.size()), curve_color, 2.5F);
    for (const auto &position : projected_controls)
    {
        draw_list.AddCircleFilled(position, 4.0F, pole_color);
    }
}
