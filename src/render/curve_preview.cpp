#include "curve_preview.hpp"

#include "curve_colors.hpp"
#include "geometry/bspline_curve.hpp"
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

struct ClipPoint
{
    double x, y, z, w;

    [[nodiscard]] constexpr bool in_depth_range() const
    {
        return z >= 0.0 && z <= w;
    }
};

// Clip a segment against one half-space before perspective division.
constexpr bool clip_to_plane(ClipPoint &a, ClipPoint &b, const double distance_a, const double distance_b)
{
    const bool outside_a = distance_a < 0.0;
    const bool outside_b = distance_b < 0.0;
    if (outside_a && outside_b)
    {
        return false;
    }
    if (outside_a != outside_b)
    {
        const double t = distance_a / (distance_a - distance_b);
        auto &outside = outside_a ? a : b;
        outside = {.x = std::lerp(a.x, b.x, t),
                   .y = std::lerp(a.y, b.y, t),
                   .z = std::lerp(a.z, b.z, t),
                   .w = std::lerp(a.w, b.w, t)};
    }
    return true;
}

constexpr bool clip_to_depth_range(ClipPoint &a, ClipPoint &b)
{
    return clip_to_plane(a, b, a.z, b.z) && clip_to_plane(a, b, a.w - a.z, b.w - b.z);
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

    [[nodiscard]] ClipPoint clip_point(const Vec3 &world) const
    {
        const Vec3 point{.x = world.x / scale, .y = world.y / scale, .z = world.z / scale};
        return {.x = clip_component(transform.x, point),
                .y = clip_component(transform.y, point),
                .z = clip_component(transform.z, point),
                .w = clip_component(transform.w, point)};
    }

    [[nodiscard]] ImVec2 screen(const ClipPoint &point) const
    {
        const double x = 0.5 + 0.5 * point.x / point.w;
        const double y = 0.5 + 0.5 * point.y / point.w;
        return {viewport.Pos.x + static_cast<float>(x * viewport.Size.x),
                viewport.Pos.y + static_cast<float>(y * viewport.Size.y)};
    }

    [[nodiscard]] std::optional<ImVec2> visible_point(const Vec3 &world) const
    {
        if (const auto point = clip_point(world); point.in_depth_range())
        {
            return screen(point);
        }
        return std::nullopt;
    }

    void draw_polyline(ImDrawList &draw_list, std::span<const Vec3> points, ImU32 color, float width) const;
};

void CurveProjection::draw_polyline(ImDrawList &draw_list, const std::span<const Vec3> points, const ImU32 color,
                                    const float width) const
{
    if (points.empty())
    {
        return;
    }
    auto previous = clip_point(points.front());
    if (previous.in_depth_range())
    {
        draw_list.PathLineTo(screen(previous));
    }
    for (const auto &point : points.subspan(1))
    {
        const auto next = clip_point(point);
        if (auto a = previous, b = next; clip_to_depth_range(a, b))
        {
            if (!previous.in_depth_range())
            {
                draw_list.PathLineTo(screen(a));
            }
            draw_list.PathLineTo(screen(b));
            if (!next.in_depth_range())
            {
                draw_list.PathStroke(color, width);
            }
        }
        previous = next;
    }
    draw_list.PathStroke(color, width);
}
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
        const auto point = project.visible_point(poles[index]);
        if (!point)
        {
            continue;
        }
        const float dx = cursor.x - point->x;
        const float dy = cursor.y - point->y;
        // At equal distance the last marker drawn is on top.
        if (const float distance_squared = dx * dx + dy * dy; distance_squared <= best_distance_squared)
        {
            best_distance_squared = distance_squared;
            hit = index;
        }
    }
    return hit;
}

auto dragged_pole_position(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                           const std::size_t index, const ImVec2 screen_delta) -> Vec3
{
    const auto &viewport = *ImGui::GetMainViewport();
    const CurveProjection project{transform, viewport, curve.framing_extent};
    const auto &origin = curve.definition.poles.at(index);
    const double w = project.clip_point(origin).w;

    // At the pole's depth w is constant. The orthogonal X/Y rows give two independent screen-plane axes.
    const auto axis_offset = [w](const ClipRow &row, const double pixels, const double size)
    {
        const gp_XYZ axis{row[0], row[1], row[2]};
        const double clip_delta = 2.0 * pixels / size * w;
        return axis * (clip_delta / axis.SquareModulus());
    };
    const auto offset = axis_offset(transform.x, screen_delta.x, viewport.Size.x) +
                        axis_offset(transform.y, screen_delta.y, viewport.Size.y);
    return {.x = origin.x + offset.X() * project.scale,
            .y = origin.y + offset.Y() * project.scale,
            .z = origin.z + offset.Z() * project.scale};
}

void draw_curve_preview(const occt_lab::geometry::SampledCurve &curve, const ClipTransform &transform,
                        const std::size_t selected_pole)
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const CurveProjection project{transform, *viewport, curve.framing_extent};

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
    if (const auto selected = project.visible_point(poles.at(selected_pole)))
    {
        draw_list.AddCircle(*selected, 7.0F, IM_COL32(255, 255, 255, 255), 0, 1.5F);
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
