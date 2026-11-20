#include "viewport_projection.hpp"

#include "orbit_camera.hpp"

#include <algorithm>
#include <cmath>

namespace occt_lab::render
{
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
    : transform_(transform), viewport_(viewport), scale_(std::max(1.0, framing_extent / 45.0))
{
}

ClipPoint ViewportProjection::clip_point(const Vec3 &world) const
{
    const Vec3 point{.x = world.x / scale_, .y = world.y / scale_, .z = world.z / scale_};
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

void ViewportProjection::draw_polyline(ImDrawList &draw_list, const std::span<const Vec3> points, const ImU32 color,
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
} // namespace occt_lab::render
