module;

#include <gp_XYZ.hxx>
#include <imgui.h>

export module formlight_geometry.render.viewport.viewport_projection;

import std;
import formlight_geometry.geometry.vec3;
import formlight_geometry.render.viewport.clip_transform;

namespace formlight_geometry::render
{
struct ClipPoint;

export class ViewportProjection
{
  public:
    ViewportProjection(const ClipTransform &transform, const ImGuiViewport &viewport, double framing_extent);
    ViewportProjection(const ClipTransform &transform, const ImGuiViewport &viewport, geometry::Vec3 center,
                       double world_units_per_view_unit);
    [[nodiscard]] std::optional<ImVec2> visible_point(const geometry::Vec3 &world) const;
    [[nodiscard]] std::optional<geometry::Vec3> xy_plane_point(ImVec2 cursor) const;
    [[nodiscard]] std::optional<std::size_t> pick_pole(std::span<const geometry::Vec3> poles, ImVec2 cursor) const;
    [[nodiscard]] geometry::Vec3 dragged_position(const geometry::Vec3 &origin, ImVec2 screen_delta) const;
    void draw_polyline(ImDrawList &draw_list, std::span<const geometry::Vec3> points, ImU32 color, float width,
                       std::size_t stride = 1) const;

  private:
    [[nodiscard]] ClipPoint clip_point(const geometry::Vec3 &world) const;
    [[nodiscard]] ImVec2 screen(const ClipPoint &point) const;

    const ClipTransform &transform_;
    const ImGuiViewport &viewport_;
    geometry::Vec3 center_;
    double scale_;
};

using geometry::Vec3;

struct ClipPoint
{
    double x, y, z, w;

    [[nodiscard]] constexpr bool in_depth_range() const
    {
        return z >= 0.0 && z <= w;
    }
};

namespace
{
bool contains_cursor(const ImGuiViewport &viewport, const ImVec2 cursor)
{
    return cursor.x >= viewport.Pos.x && cursor.x < viewport.Pos.x + viewport.Size.x && cursor.y >= viewport.Pos.y &&
           cursor.y < viewport.Pos.y + viewport.Size.y;
}

double clip_component(const ClipRow &row, const Vec3 &point)
{
    return row[0] * point.x + row[1] * point.y + row[2] * point.z + row[3];
}

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
} // namespace

ViewportProjection::ViewportProjection(const ClipTransform &transform, const ImGuiViewport &viewport,
                                       const double framing_extent)
    : ViewportProjection(transform, viewport, {}, std::max(1.0, framing_extent / 45.0))
{
}

ViewportProjection::ViewportProjection(const ClipTransform &transform, const ImGuiViewport &viewport, const Vec3 center,
                                       const double world_units_per_view_unit)
    : transform_(transform), viewport_(viewport), center_(center), scale_(world_units_per_view_unit)
{
}

ClipPoint ViewportProjection::clip_point(const Vec3 &world) const
{
    const Vec3 point{
        .x = (world.x - center_.x) / scale_, .y = (world.y - center_.y) / scale_, .z = (world.z - center_.z) / scale_};
    return {.x = clip_component(transform_.x, point),
            .y = clip_component(transform_.y, point),
            .z = clip_component(transform_.z, point),
            .w = clip_component(transform_.w, point)};
}

ImVec2 ViewportProjection::screen(const ClipPoint &point) const
{
    const double x = 0.5 + 0.5 * point.x / point.w;
    const double y = 0.5 + 0.5 * point.y / point.w;
    return {viewport_.Pos.x + static_cast<float>(x * viewport_.Size.x),
            viewport_.Pos.y + static_cast<float>(y * viewport_.Size.y)};
}

std::optional<ImVec2> ViewportProjection::visible_point(const Vec3 &world) const
{
    if (const auto point = clip_point(world); point.in_depth_range())
    {
        return screen(point);
    }
    return std::nullopt;
}

std::optional<Vec3> ViewportProjection::xy_plane_point(const ImVec2 cursor) const
{
    if (!contains_cursor(viewport_, cursor))
    {
        return std::nullopt;
    }
    const double ndc_x = 2.0 * (cursor.x - viewport_.Pos.x) / viewport_.Size.x - 1.0;
    const double ndc_y = 2.0 * (cursor.y - viewport_.Pos.y) / viewport_.Size.y - 1.0;
    const auto hit = transform_.intersect_z_plane(ndc_x, ndc_y, -center_.z / scale_);
    if (hit)
    {
        const Vec3 world{hit->x * scale_ + center_.x, hit->y * scale_ + center_.y, 0};
        if (std::isfinite(world.x) && std::isfinite(world.y))
        {
            return world;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ViewportProjection::pick_pole(const std::span<const Vec3> poles, const ImVec2 cursor) const
{
    if (!contains_cursor(viewport_, cursor))
    {
        return std::nullopt;
    }

    constexpr float hit_radius = 9.0F;
    float best_distance_squared = hit_radius * hit_radius;
    std::optional<std::size_t> hit;
    for (std::size_t index = 0; index < poles.size(); ++index)
    {
        const auto point = visible_point(poles[index]);
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

void ViewportProjection::draw_polyline(ImDrawList &draw_list, const std::span<const Vec3> points, const ImU32 color,
                                       const float width, const std::size_t stride) const
{
    if (points.empty() || stride == 0)
    {
        return;
    }
    auto previous = clip_point(points.front());
    if (previous.in_depth_range())
    {
        draw_list.PathLineTo(screen(previous));
    }
    for (std::size_t index = stride; index < points.size(); index += stride)
    {
        const auto next = clip_point(points[index]);
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

Vec3 ViewportProjection::dragged_position(const Vec3 &origin, const ImVec2 screen_delta) const
{
    const double w = clip_point(origin).w;

    // At the pole's depth w is constant. The orthogonal X/Y rows give two independent screen-plane axes.
    const auto axis_offset = [w](const ClipRow &row, const double pixels, const double size)
    {
        const gp_XYZ axis{row[0], row[1], row[2]};
        const double clip_delta = 2.0 * pixels / size * w;
        return axis * (clip_delta / axis.SquareModulus());
    };
    const auto offset = axis_offset(transform_.x, screen_delta.x, viewport_.Size.x) +
                        axis_offset(transform_.y, screen_delta.y, viewport_.Size.y);
    return {
        .x = origin.x + offset.X() * scale_, .y = origin.y + offset.Y() * scale_, .z = origin.z + offset.Z() * scale_};
}
} // namespace formlight_geometry::render
