module;

#include <imgui.h>

export module occt_lab.ui.curve.curve_editor;

import std;
import occt_lab.application.editable_curve;
import occt_lab.application.nurbs_curve_creation;
import occt_lab.document.curve_project;
import occt_lab.geometry.bspline_knot;
import occt_lab.geometry.vec3;
import occt_lab.render.preview.curve_preview;
import occt_lab.render.viewport.clip_transform;
import occt_lab.ui.curve.fit_residuals;
import occt_lab.ui.pole_drag;
import occt_lab.ui.project_file_controls;

export class CurveEditor
{
  public:
    explicit CurveEditor(const EditableCurve &curve) : draft_(curve.current().definition)
    {
    }
    void draw(EditableCurve &curve);
    void draw_preview(const EditableCurve &curve, const ClipTransform &transform) const;
    void handle_viewport_input(EditableCurve &curve, const ClipTransform &transform, bool pole_click);
    void cancel_drag(EditableCurve &curve);
    void cancel_point_picking()
    {
        picking_points_ = false;
        creation_preview_.reset();
    }
    [[nodiscard]] bool picking_points() const
    {
        return picking_points_;
    }
    [[nodiscard]] bool tracking_pole() const
    {
        return tracking_pole_;
    }

  private:
    enum CreationPreset
    {
        custom_line,
        cubic_bezier,
        interpolate_points,
        control_points,
        approximate_points
    };

    [[nodiscard]] std::size_t selected_index() const
    {
        return static_cast<std::size_t>(settings_.pole_number - 1);
    }

    void draw_project(EditableCurve &curve);
    void draw_creation_dialog(EditableCurve &curve);
    auto create_from_draft(EditableCurve &curve, EditableCurve::Framing framing) -> std::expected<void, std::string>;
    bool draw_control_parameters();
    bool draw_point_parameters();
    void draw_creation_fit_summary() const;
    void refresh_creation_preview(const EditableCurve &curve);
    void draw_point_picker(EditableCurve &curve);
    void preview_point_picking(const EditableCurve &curve, const ClipTransform &transform, bool scene_click);
    void draw_history(EditableCurve &curve);
    void draw_refinement(EditableCurve &curve);
    void draw_point_fitting(EditableCurve &curve);
    void finish_curve_operation(const EditableCurve &curve, const std::expected<void, std::string> &result);
    EditableCurve::Definition draft_;
    LineCurveParameters creation_parameters_;
    std::vector<occt_lab::geometry::Vec3> creation_points_;
    bool picking_points_ = false;
    int creation_preset_ = custom_line;
    int control_degree_ = 3;
    std::optional<std::vector<occt_lab::geometry::BSplineKnot>> control_knots_;
    std::optional<EditableCurve::Curve> creation_preview_;
    double creation_tolerance_ = 1.0;
    std::string creation_error_;
    occt_lab::document::CurveProjectSettings settings_;
    ProjectFileControls project_file_{.path = "curve.occtlab"};
    bool tracking_pole_ = false;
    std::string error_;
};

void CurveEditor::cancel_drag(EditableCurve &curve)
{
    if (tracking_pole_)
    {
        curve.cancel_pole_move();
        draft_.poles[selected_index()] = curve.current().definition.poles[selected_index()];
        tracking_pole_ = false;
    }
}

void CurveEditor::handle_viewport_input(EditableCurve &curve, const ClipTransform &transform, const bool pole_click)
{
    if (picking_points_)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            cancel_point_picking();
        }
        else
        {
            preview_point_picking(curve, transform, pole_click);
        }
        return;
    }
    if (!tracking_pole_)
    {
        if (!pole_click)
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

    const auto motion = read_pole_drag();
    if (!motion)
    {
        cancel_drag(curve);
        return;
    }

    const auto index = selected_index();
    const auto position = dragged_pole_position(curve.current(), transform, index, motion->delta);
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
    if (motion->released)
    {
        curve.accept_pole_move();
        tracking_pole_ = false;
    }
}

void CurveEditor::draw_preview(const EditableCurve &curve, const ClipTransform &transform) const
{
    if (creation_preview_)
    {
        draw_curve_preview(*creation_preview_, transform);
    }
    else if (!picking_points_)
    {
        draw_curve_preview(curve.displayed(), transform, selected_index());
    }
}

void CurveEditor::draw(EditableCurve &curve)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("NURBS curve control points", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (picking_points_)
        {
            draw_point_picker(curve);
            ImGui::End();
            return;
        }
        ImGui::BeginDisabled(tracking_pole_);
        draw_project(curve);
        ImGui::BeginDisabled(draft_ != curve.current().definition);
        draw_creation_dialog(curve);
        ImGui::EndDisabled();
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
        const auto &knots = current.definition.knots;
        for (std::size_t i = 0; i < knots.size(); ++i)
        {
            const auto [value, multiplicity] = knots[i];
            if (i == 0 || i + 1 == knots.size())
            {
                ImGui::Text("%.12g  (x%d, endpoint)", value, multiplicity);
            }
            else
            {
                ImGui::Text("%.12g  (x%d, at least C%d)", value, multiplicity,
                            current.definition.degree - multiplicity);
            }
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
