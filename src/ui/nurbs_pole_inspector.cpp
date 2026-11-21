#include "nurbs_pole_inspector.hpp"

#include "geometry/surfaces/bspline_surface.hpp"

#include <imgui.h>

#include <limits>

void draw_nurbs_pole_inspector(const occt_lab::geometry::BSplineSurfaceDefinition &surface,
                               const std::size_t selected_pole)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    constexpr char title[] = "NURBS control point";
    const auto &style = ImGui::GetStyle();
    // Reserve a frame for the collapse button even when the selected pole has short coordinates.
    const float min_width = ImGui::CalcTextSize(title).x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                            2.0F * style.FramePadding.x;
    constexpr float unlimited = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints({min_width, 0.0F}, {unlimited, unlimited});
    if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto columns = static_cast<std::size_t>(surface.u.pole_count);
        const auto &[x, y, z] = surface.poles.at(selected_pole);
        const double weight = surface.weights.empty() ? 1.0 : surface.weights.at(selected_pole);
        ImGui::Text("Pole U %zu   V %zu", selected_pole % columns + 1, selected_pole / columns + 1);
        ImGui::Text("XYZ (mm)   (%.8g, %.8g, %.8g)", x, y, z);
        ImGui::Text("Weight   %.8g", weight);
    }
    ImGui::End();
}
