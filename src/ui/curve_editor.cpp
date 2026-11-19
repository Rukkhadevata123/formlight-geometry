#include "curve_editor.hpp"

#include <imgui.h>

void CurveEditor::draw(EditableCurve &curve)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("B-spline control points", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushItemWidth(9.0F * ImGui::GetFontSize());
        ImGui::SliderInt("Pole", &pole_number_, 1, static_cast<int>(draft_.poles.size()), "%d",
                         ImGuiSliderFlags_AlwaysClamp);
        const auto index = selected_index();
        auto &[draft_x, draft_y, draft_z] = draft_.poles[index];
        ImGui::PushID(pole_number_);
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        constexpr auto flags = ImGuiInputTextFlags_ParseEmptyRefVal;
        const bool x_changed = ImGui::InputDouble("X (mm)", &draft_x, 0.0, 0.0, "%.8g", flags);
        const bool y_changed = ImGui::InputDouble("Y (mm)", &draft_y, 0.0, 0.0, "%.8g", flags);
        const bool z_changed = ImGui::InputDouble("Z (mm)", &draft_z, 0.0, 0.0, "%.8g", flags);
        const bool weight_changed = ImGui::InputDouble("Weight", &draft_.weights[index], 0.0, 0.0, "%.8g", flags);
        ImGui::PopItemFlag();
        ImGui::PopID();
        ImGui::PopItemWidth();
        if (x_changed || y_changed || z_changed || weight_changed)
        {
            error_.clear();
        }

        ImGui::BeginDisabled(draft_ == curve.current().definition);
        if (ImGui::Button("Apply"))
        {
            const auto result = curve.apply(draft_);
            error_ = result ? std::string{} : result.error();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            draft_ = curve.current().definition;
            error_.clear();
        }
        ImGui::EndDisabled();

        const bool clean = draft_ == curve.current().definition;
        ImGui::BeginDisabled(!curve.can_undo());
        const bool undo = ImGui::Button("Undo");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!curve.can_redo());
        const bool redo = ImGui::Button("Redo");
        ImGui::EndDisabled();
        if (undo || redo)
        {
            if (undo)
            {
                curve.undo();
            }
            else
            {
                curve.redo();
            }
            if (clean)
            {
                draft_ = curve.current().definition;
            }
            error_.clear();
        }
        const auto &[poles, weights] = curve.current().definition;
        const auto &[x, y, z] = poles[index];
        ImGui::Text("Applied: (%.8g, %.8g, %.8g) mm", x, y, z);
        ImGui::Text("Applied weight: %.8g", weights[index]);
        if (curve.current().unit_weight_samples)
        {
            ImGui::TextUnformatted("Gold: weighted   Gray: all weights = 1");
        }
        if (!error_.empty())
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 20.0F * ImGui::GetFontSize());
            ImGui::Text("Apply failed: %s", error_.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
}
