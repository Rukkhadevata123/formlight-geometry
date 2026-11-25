module;

#include <imgui.h>

export module formlight_geometry.ui.pole_drag;

import std;

export struct PoleDragMotion
{
    ImVec2 delta;
    bool released;
};

// Called after picking a pole. ImGui keeps the total drag delta through the release frame.
export [[nodiscard]] inline std::optional<PoleDragMotion> read_pole_drag()
{
    constexpr auto button = ImGuiMouseButton_Left;
    const bool released = ImGui::IsMouseReleased(button);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsMousePosValid() || (!ImGui::IsMouseDown(button) && !released))
    {
        return std::nullopt;
    }
    return PoleDragMotion{.delta = ImGui::GetMouseDragDelta(button), .released = released};
}
