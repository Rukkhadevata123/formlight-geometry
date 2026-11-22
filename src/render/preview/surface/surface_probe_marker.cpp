#include "surface_probe_marker.hpp"

#include <algorithm>
#include <array>
#include <cmath>

using occt_lab::geometry::Vec3;

void draw_surface_probe_marker(ImDrawList &draw_list, const occt_lab::render::ViewportProjection &project,
                               const Vec3 point, const std::optional<Vec3> &normal, const double arrow_length)
{
    const auto base = project.visible_point(point);
    if (!base)
    {
        return;
    }
    constexpr ImU32 color = IM_COL32(255, 150, 118, 255);
    if (normal)
    {
        const Vec3 tip = point + *normal * arrow_length;
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
                draw_list.AddTriangleFilled(*end, {back.x + wing.x, back.y + wing.y},
                                            {back.x - wing.x, back.y - wing.y}, color);
            }
        }
    }
    draw_list.AddCircleFilled(*base, 4.0F, color);
    draw_list.AddCircle(*base, 6.0F, IM_COL32_WHITE, 0, 1.5F);
}
