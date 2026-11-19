#include "curve_editor.hpp"

#include <imgui.h>

void CurveEditor::draw(EditableCurve &curve)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("B-spline control points (mm)", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushItemWidth(9.0F * ImGui::GetFontSize());
        ImGui::SliderInt("Pole", &pole_number_, 1, static_cast<int>(draft_.size()), "%d", ImGuiSliderFlags_AlwaysClamp);
        auto &[draft_x, draft_y, draft_z] = draft_[selected_index()];
        ImGui::PushID(pole_number_);
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        constexpr auto flags = ImGuiInputTextFlags_ParseEmptyRefVal;
        const bool x_changed = ImGui::InputDouble("X", &draft_x, 0.0, 0.0, "%.8g", flags);
        const bool y_changed = ImGui::InputDouble("Y", &draft_y, 0.0, 0.0, "%.8g", flags);
        const bool z_changed = ImGui::InputDouble("Z", &draft_z, 0.0, 0.0, "%.8g", flags);
        ImGui::PopItemFlag();
        ImGui::PopID();
        ImGui::PopItemWidth();
        if (x_changed || y_changed || z_changed)
        {
            error_.clear();
        }

        ImGui::BeginDisabled(draft_ == curve.current().poles);
        if (ImGui::Button("Apply"))
        {
            const auto result = curve.apply(draft_);
            error_ = result ? std::string{} : result.error();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            draft_ = curve.current().poles;
            error_.clear();
        }
        ImGui::EndDisabled();

        const bool clean = draft_ == curve.current().poles;
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
                draft_ = curve.current().poles;
            }
            error_.clear();
        }
        const auto &[x, y, z] = curve.current().poles[selected_index()];
        ImGui::Text("Applied: %.8g, %.8g, %.8g", x, y, z);
        if (!error_.empty())
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 20.0F * ImGui::GetFontSize());
            ImGui::Text("Apply failed: %s", error_.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
}
