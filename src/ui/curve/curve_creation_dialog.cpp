#include "curve_editor.hpp"

#include <imgui.h>

#include <algorithm>

namespace
{
enum CreationPreset
{
    custom_line,
    cubic_bezier
};

bool draw_line_parameters(LineCurveParameters &parameters)
{
    auto &[count, degree] = parameters;
    ImGui::TextUnformatted("Start with evenly spaced control points along X (90 mm).");
    constexpr auto flags = ImGuiSliderFlags_AlwaysClamp;
    const bool count_changed =
        ImGui::SliderInt("Poles", &count, LineCurveParameters::min_poles, LineCurveParameters::max_poles, "%d", flags);
    const int limit = LineCurveParameters::degree_limit(count);
    degree = std::min(degree, limit);
    ImGui::BeginDisabled(limit == 1);
    const bool degree_changed = ImGui::SliderInt("Degree", &degree, 1, limit, "%d", flags);
    ImGui::EndDisabled();
    return count_changed || degree_changed;
}
} // namespace

void CurveEditor::draw_creation_dialog(EditableCurve &curve)
{
    constexpr char title[] = "New curve";
    if (ImGui::Button("New curve..."))
    {
        creation_parameters_ = {};
        creation_preset_ = custom_line;
        creation_error_.clear();
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    ImGui::PushItemWidth(12.0F * ImGui::GetFontSize());
    const bool preset_changed = ImGui::Combo("Preset", &creation_preset_, "Custom line\0Cubic Bezier\0");
    const bool parameters_changed = creation_preset_ == custom_line && draw_line_parameters(creation_parameters_);
    if (creation_preset_ == cubic_bezier)
    {
        ImGui::TextUnformatted("Four control points form a cubic arc in the XY plane.");
    }
    ImGui::PopItemWidth();
    if (preset_changed || parameters_changed)
    {
        creation_error_.clear();
    }

    if (ImGui::Button("Create"))
    {
        const auto result =
            creation_preset_ == cubic_bezier ? curve.create_cubic_bezier() : curve.create_line(creation_parameters_);
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
