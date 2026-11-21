#include "surface_wireframe.hpp"

using occt_lab::geometry::PatchIsolines;
using occt_lab::geometry::Vec3;
using occt_lab::render::ViewportProjection;

void draw_surface_wireframe(ImDrawList &draw_list, const ViewportProjection &project, const std::span<const Vec3> net,
                            const std::size_t u_count, const PatchIsolines &isolines, const PatchLayers &layers,
                            const std::size_t selected_pole)
{
    constexpr ImU32 net_color = IM_COL32(111, 178, 216, 210);
    if (layers.control_net)
    {
        IM_ASSERT(u_count > 0 && net.size() >= u_count && net.size() % u_count == 0);
        for (std::size_t start = 0; start < net.size(); start += u_count)
        {
            project.draw_polyline(draw_list, net.subspan(start, u_count), net_color, 1.25F);
        }
        for (std::size_t column = 0; column < u_count; ++column)
        {
            project.draw_polyline(draw_list, net.subspan(column), net_color, 1.25F, u_count);
        }
    }
    if (layers.constant_u)
    {
        for (const auto &line : isolines.constant_u)
        {
            project.draw_polyline(draw_list, line, IM_COL32(255, 185, 43, 255), 2.0F);
        }
    }
    if (layers.constant_v)
    {
        for (const auto &line : isolines.constant_v)
        {
            project.draw_polyline(draw_list, line, IM_COL32(104, 226, 153, 255), 2.0F);
        }
    }
    if (layers.control_net)
    {
        for (const auto &pole : net)
        {
            if (const auto point = project.visible_point(pole))
            {
                draw_list.AddCircleFilled(*point, 4.0F, net_color);
            }
        }
        IM_ASSERT(selected_pole < net.size());
        if (const auto point = project.visible_point(net[selected_pole]))
        {
            draw_list.AddCircle(*point, 7.0F, IM_COL32_WHITE, 0, 1.5F);
        }
    }
}
