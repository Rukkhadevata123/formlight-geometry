#include "curve_editor.hpp"

#include <imgui.h>

#include <algorithm>

void CurveEditor::draw_creation_dialog(EditableCurve &curve)
{
    constexpr char title[] = "New curve";
    if (ImGui::Button("New curve..."))
    {
        creation_parameters_ = {};
        creation_error_.clear();
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    auto &[count, degree] = creation_parameters_;
    ImGui::TextUnformatted("Start with evenly spaced control points along X (90 mm).");
    ImGui::PushItemWidth(12.0F * ImGui::GetFontSize());
    constexpr auto flags = ImGuiSliderFlags_AlwaysClamp;
    const bool count_changed =
        ImGui::SliderInt("Poles", &count, LineCurveParameters::min_poles, LineCurveParameters::max_poles, "%d", flags);
    const int limit = LineCurveParameters::degree_limit(count);
    degree = std::min(degree, limit);
    ImGui::BeginDisabled(limit == 1);
    const bool degree_changed = ImGui::SliderInt("Degree", &degree, 1, limit, "%d", flags);
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    if (count_changed || degree_changed)
    {
        creation_error_.clear();
    }

    if (ImGui::Button("Create"))
    {
        const auto result = curve.create_line(creation_parameters_);
        if (result)
        {
            finish_curve_operation(curve, result);
            ImGui::CloseCurrentPopup();
        }
        else
        {
            creation_error_ = result.error();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ImGui::CloseCurrentPopup();
    }
    if (!creation_error_.empty())
    {
        ImGui::TextUnformatted(creation_error_.c_str());
    }
    ImGui::EndPopup();
}
