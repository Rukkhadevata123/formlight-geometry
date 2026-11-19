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

        draw_history(curve);
        const auto &applied = curve.current().definition;
        const auto &[x, y, z] = applied.poles[selected_index()];
        ImGui::Text("Applied: (%.8g, %.8g, %.8g) mm", x, y, z);
        ImGui::Text("Applied weight: %.8g", applied.weights[selected_index()]);
        if (curve.current().unit_weight_samples)
        {
            ImGui::TextUnformatted("Gold: weighted   Gray: all weights = 1");
        }
        draw_refinement(curve);
        if (!error_.empty())
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 20.0F * ImGui::GetFontSize());
            ImGui::TextUnformatted(error_.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
}

void CurveEditor::draw_history(EditableCurve &curve)
{
    const bool clean = draft_ == curve.current().definition;
    const bool changes_knots = curve.history_changes_knots();
    const bool preserve_draft = !clean && changes_knots;
    ImGui::BeginDisabled(!curve.can_undo() || preserve_draft);
    const bool undo = ImGui::Button("Undo");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!curve.can_redo() || preserve_draft);
    const bool redo = ImGui::Button("Redo");
    ImGui::EndDisabled();
    if (!undo && !redo)
    {
        return;
    }
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
    if (changes_knots)
    {
        pole_number_ = 1;
    }
    error_.clear();
}

void CurveEditor::draw_refinement(EditableCurve &curve)
{
    ImGui::SeparatorText("Curve refinement");
    ImGui::SetNextItemWidth(9.0F * ImGui::GetFontSize());
    if (ImGui::InputDouble("Parameter u", &knot_parameter_, 0.0, 0.0, "%.12g"))
    {
        error_.clear();
    }
    const bool dirty = draft_ != curve.current().definition;
    ImGui::BeginDisabled(dirty);
    const bool insert = ImGui::Button("Insert knot");
    ImGui::SameLine();
    const bool elevate = ImGui::Button("Elevate degree (+1)");
    ImGui::EndDisabled();
    if (insert || elevate)
    {
        const auto result =
            insert ? curve.insert_knot(knot_parameter_) : curve.elevate_degree(curve.current().definition.degree + 1);
        if (result)
        {
            draft_ = curve.current().definition;
            pole_number_ = 1;
            error_.clear();
        }
        else
        {
            error_ = result.error();
        }
    }
    if (dirty)
    {
        ImGui::TextUnformatted("Apply or cancel pole edits before refining.");
    }
    const auto &current = curve.current();
    if (current.refinement)
    {
        const auto &[previous_degree, previous_poles, deviation] = *current.refinement;
        ImGui::Text("Degree %d -> %d    Poles %zu -> %zu", previous_degree, current.definition.degree, previous_poles,
                    current.definition.poles.size());
        ImGui::Text("Max deviation (%zu samples): %.3g mm", current.samples.size(), deviation);
    }
    else
    {
        ImGui::Text("Degree %d    Poles %zu", current.definition.degree, current.definition.poles.size());
    }
    ImGui::Text("%zu distinct knots", current.definition.knots.size());
    if (ImGui::TreeNode("Knots and multiplicities"))
    {
        for (const auto &[value, multiplicity] : current.definition.knots)
        {
            ImGui::Text("%.12g  (x%d)", value, multiplicity);
        }
        ImGui::TreePop();
    }
}
