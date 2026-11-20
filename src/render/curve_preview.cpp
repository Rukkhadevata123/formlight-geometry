#include "curve_preview.hpp"

#include "curve_colors.hpp"
#include "geometry/bspline_curve.hpp"
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

struct CurveProjection
{
    static constexpr double half_extent = 45.0;
    const ClipTransform &transform;
    const ImGuiViewport &viewport;
    double scale;

    CurveProjection(const ClipTransform &transform, const ImGuiViewport &viewport, const double framing_extent)
        : transform(transform), viewport(viewport), scale(std::max(1.0, framing_extent / half_extent))
    {
    }

    [[nodiscard]] ImVec2 operator()(const Vec3 &world) const
    {
        const Vec3 point{.x = world.x / scale, .y = world.y / scale, .z = world.z / scale};
        const double w = clip_component(transform.w, point);
        const double x = 0.5 + 0.5 * clip_component(transform.x, point) / w;
        const double y = 0.5 + 0.5 * clip_component(transform.y, point) / w;
        return {viewport.Pos.x + static_cast<float>(x * viewport.Size.x),
                viewport.Pos.y + static_cast<float>(y * viewport.Size.y)};
    }
};
} // namespace

auto pick_curve_pole(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform, const ImVec2 cursor)
    -> std::optional<std::size_t>
{
    const auto &viewport = *ImGui::GetMainViewport();
    if (cursor.x < viewport.Pos.x || cursor.x >= viewport.Pos.x + viewport.Size.x || cursor.y < viewport.Pos.y ||
        cursor.y >= viewport.Pos.y + viewport.Size.y)
    {
        return std::nullopt;
    }

    const CurveProjection project{transform, viewport, curve.framing_extent};
    constexpr float hit_radius = 9.0F;
    float best_distance_squared = hit_radius * hit_radius;
    std::optional<std::size_t> hit;
    const auto &poles = curve.definition.poles;
    for (std::size_t index = 0; index < poles.size(); ++index)
    {
        const auto point = project(poles[index]);
        const float dx = cursor.x - point.x;
        const float dy = cursor.y - point.y;
        // At equal distance the last marker drawn is on top.
        if (const float distance_squared = dx * dx + dy * dy; distance_squared <= best_distance_squared)
        {
            best_distance_squared = distance_squared;
            hit = index;
        }
    }
    return hit;
}

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        const std::size_t selected_pole)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const CurveProjection project{transform, *viewport, curve.framing_extent};
    const auto draw_polyline = [&](const std::span<const Vec3> points, const ImU32 color, const float width)
    {
        for (const auto &point : points)
        {
            draw_list.PathLineTo(project(point));
        }
        draw_list.PathStroke(color, width);
    };

    constexpr ImU32 polygon_color = IM_COL32(80, 145, 185, 225);
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
        draw_polyline(curve.approximation->interpolated_samples, curve_colors::interpolant_reference, 2.0F);
    }
    draw_polyline(curve.samples, curve_colors::current, 2.5F);
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
