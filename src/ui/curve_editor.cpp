#include "curve_editor.hpp"
#include "fit_residuals.hpp"
#include "render/curve_preview.hpp"

#include <imgui.h>

void CurveEditor::cancel_drag(EditableCurve &curve)
{
    if (tracking_pole_)
    {
        curve.cancel_pole_move();
        draft_.poles[selected_index()] = curve.current().definition.poles[selected_index()];
        tracking_pole_ = false;
    }
}

void CurveEditor::handle_viewport_input(EditableCurve &curve, const ClipTransform &transform)
{
    constexpr auto button = ImGuiMouseButton_Left;
    if (!tracking_pole_)
    {
        if (const auto &io = ImGui::GetIO(); !ImGui::IsMouseClicked(button) || io.WantCaptureMouse ||
                                             io.KeyMods != ImGuiMod_None || ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            return;
        }
        if (const auto hit = pick_curve_pole(curve.current(), transform, ImGui::GetMousePos()))
        {
            settings_.pole_number = static_cast<int>(*hit) + 1;
            tracking_pole_ = draft_ == curve.current().definition;
        }
        return;
    }

    const bool released = ImGui::IsMouseReleased(button);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsMousePosValid() || (!ImGui::IsMouseDown(button) && !released))
    {
        cancel_drag(curve);
        return;
    }

    // ImGui keeps the press-to-cursor delta on the release frame and filters ordinary click jitter.
    const auto delta = ImGui::GetMouseDragDelta(button);
    const auto index = selected_index();
    const auto position = dragged_pole_position(curve.current(), transform, index, delta);
    if (const auto result = curve.preview_pole_move(index, position); !result)
    {
        error_ = result.error();
        cancel_drag(curve);
        return;
    }
    if (draft_.poles[index] != position)
    {
        draft_.poles[index] = position;
        error_.clear();
    }
    if (released)
    {
        curve.accept_pole_move();
        tracking_pole_ = false;
    }
}

void CurveEditor::draw(EditableCurve &curve)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("B-spline control points", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(tracking_pole_);
        draw_project(curve);
        ImGui::PushItemWidth(9.0F * ImGui::GetFontSize());
        ImGui::SliderInt("Pole", &settings_.pole_number, 1, static_cast<int>(draft_.poles.size()), "%d",
                         ImGuiSliderFlags_AlwaysClamp);
        const auto index = selected_index();
        auto &[draft_x, draft_y, draft_z] = draft_.poles[index];
        ImGui::PushID(settings_.pole_number);
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
        if (curve.displayed().unit_weight_samples)
        {
            ImGui::TextUnformatted("Gold: weighted   Gray: all weights = 1");
        }
        draw_refinement(curve);
        draw_point_fitting(curve);
        ImGui::EndDisabled();
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
        settings_.pole_number = 1;
    }
    error_.clear();
}

void CurveEditor::draw_refinement(EditableCurve &curve)
{
    ImGui::SeparatorText("Curve refinement");
    ImGui::SetNextItemWidth(9.0F * ImGui::GetFontSize());
    if (ImGui::InputDouble("Parameter u", &settings_.knot_parameter, 0.0, 0.0, "%.12g"))
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
        const auto result = insert ? curve.insert_knot(settings_.knot_parameter)
                                   : curve.elevate_degree(curve.current().definition.degree + 1);
        finish_curve_operation(curve, result);
    }
    if (dirty && !tracking_pole_)
    {
        ImGui::TextUnformatted("Apply or cancel edits before dragging or running curve operations.");
    }
    const auto &current = curve.displayed();
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

void CurveEditor::finish_curve_operation(const EditableCurve &curve, const std::expected<void, std::string> &result)
{
    if (!result)
    {
        error_ = result.error();
        return;
    }
    draft_ = curve.current().definition;
    settings_.pole_number = 1;
    error_.clear();
}

void CurveEditor::draw_point_fitting(EditableCurve &curve)
{
    ImGui::SeparatorText("Point fitting");
    ImGui::BeginDisabled(draft_ != curve.current().definition);
    if (ImGui::Button("Interpolate through applied poles"))
    {
        const auto result = curve.interpolate_through_poles();
        finish_curve_operation(curve, result);
    }
    ImGui::SetNextItemWidth(9.0F * ImGui::GetFontSize());
    if (ImGui::InputDouble("Target tolerance (mm)", &settings_.fit_tolerance, 0.0, 0.0, "%.8g"))
    {
        error_.clear();
    }
    if (const bool use_poles = curve.current().data_points.empty();
        ImGui::Button(use_poles ? "Approximate applied poles" : "Approximate same data points"))
    {
        const auto result = curve.approximate_data_points(settings_.fit_tolerance);
        finish_curve_operation(curve, result);
    }
    ImGui::EndDisabled();

    draw_fit_residuals(curve.displayed());
}
