#pragma once

#include "application/editable_curve.hpp"
#include "document/curve_project.hpp"
#include "ui/project_file_controls.hpp"

struct ClipTransform;

class CurveEditor
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
        control_preview_.reset();
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
        control_points
    };

    [[nodiscard]] std::size_t selected_index() const
    {
        return static_cast<std::size_t>(settings_.pole_number - 1);
    }

    void draw_project(EditableCurve &curve);
    void draw_creation_dialog(EditableCurve &curve);
    auto create_from_draft(EditableCurve &curve, EditableCurve::Framing framing) -> std::expected<void, std::string>;
    bool draw_control_degree();
    void refresh_control_preview(const EditableCurve &curve);
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
    std::optional<EditableCurve::Curve> control_preview_;
    std::string creation_error_;
    occt_lab::document::CurveProjectSettings settings_;
    ProjectFileControls project_file_{.path = "curve.occtlab"};
    bool tracking_pole_ = false;
    std::string error_;
};
