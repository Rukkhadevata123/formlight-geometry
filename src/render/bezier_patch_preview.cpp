#include "bezier_patch_preview.hpp"

#include "viewport_projection.hpp"

#include <array>
#include <stdexcept>

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::PatchIsolines;
using occt_lab::geometry::Vec3;
constexpr auto side = CubicBezierPatch::side_length;
constexpr double half_width = 45.0;

CubicBezierPatch::ControlNet make_control_net()
{
    constexpr std::array<std::array<double, side>, side> heights{{
        {0, 0, 0, 0},
        {0, 40, 28, 0},
        {0, 14, 32, 0},
        {0, 0, 0, 0},
    }};
    CubicBezierPatch::ControlNet net{};
    for (std::size_t row = 0; row < heights.size(); ++row)
    {
        constexpr double half_height = 36.0;
        const double y = half_height * (2.0 * static_cast<double>(row) / (side - 1) - 1.0);
        for (std::size_t col = 0; col < side; ++col)
        {
            const double x = half_width * (2.0 * static_cast<double>(col) / (side - 1) - 1.0);
            net[row * side + col] = {.x = x, .y = y, .z = heights[row][col]};
        }
    }
    return net;
}

PatchIsolines make_isolines(const CubicBezierPatch::ControlNet &net)
{
    const auto patch = CubicBezierPatch::create(net);
    if (!patch)
    {
        throw std::runtime_error("Invalid Bezier preview control net");
    }
    const auto lines = sample_patch_isolines(*patch);
    if (!lines)
    {
        throw std::runtime_error("Could not sample the Bezier preview isolines");
    }
    return *lines;
}
} // namespace

BezierPatchPreview::BezierPatchPreview() : control_net_(make_control_net()), isolines_(make_isolines(control_net_))
{
}

void BezierPatchPreview::draw(const ClipTransform &transform, const PatchLayers &layers) const
{
    auto *viewport = ImGui::GetMainViewport();
    auto &draw_list = *ImGui::GetBackgroundDrawList(viewport);
    const occt_lab::render::ViewportProjection project{transform, *viewport, half_width};
    constexpr ImU32 net_color = IM_COL32(111, 178, 216, 210);
    if (layers.control_net)
    {
        const std::span net{control_net_};
        for (std::size_t i = 0; i < side; ++i)
        {
            project.draw_polyline(draw_list, net.subspan(i * side, side), net_color, 1.25F);
            std::array<Vec3, side> column{};
            for (std::size_t row = 0; row < side; ++row)
            {
                column[row] = net[row * side + i];
            }
            project.draw_polyline(draw_list, column, net_color, 1.25F);
        }
    }
    if (layers.constant_u)
    {
        for (const auto &line : isolines_.constant_u)
        {
            project.draw_polyline(draw_list, line, IM_COL32(255, 185, 43, 255), 2.0F);
        }
    }
    if (layers.constant_v)
    {
        for (const auto &line : isolines_.constant_v)
        {
            project.draw_polyline(draw_list, line, IM_COL32(104, 226, 153, 255), 2.0F);
        }
    }
    if (layers.control_net)
    {
        for (const auto &pole : control_net_)
        {
            if (const auto point = project.visible_point(pole))
            {
                draw_list.AddCircleFilled(*point, 4.0F, net_color);
            }
        }
    }
}
