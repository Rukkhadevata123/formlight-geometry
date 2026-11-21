#include "developer_diagnostics.hpp"

#include <imgui.h>

bool draw_developer_diagnostics(bool &open, const bool compute_supported, const std::string &compute_status)
{
    if (!open)
    {
        return false;
    }

    bool run_check = false;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, {0.5F, 0.5F});
    if (ImGui::Begin("Developer diagnostics", &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SeparatorText("Vulkan compute");
        if (compute_supported)
        {
            ImGui::TextUnformatted("out[i] = 3*i + 7, 2 groups x 64 lanes");
            run_check = ImGui::Button("Run buffer check");
            if (!compute_status.empty())
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 20.0F * ImGui::GetFontSize());
                ImGui::TextUnformatted(compute_status.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        else
        {
            ImGui::TextUnformatted("Graphics queue has no compute support.");
        }
    }
    ImGui::End();
    return run_check;
}
