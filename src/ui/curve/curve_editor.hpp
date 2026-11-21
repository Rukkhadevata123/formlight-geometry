#pragma once

#include "application/editable_curve.hpp"
#include "document/curve_project.hpp"

struct ClipTransform;

class CurveEditor
{
  public:
    explicit CurveEditor(const EditableCurve &curve) : draft_(curve.current().definition)
    {
    }
    void draw(EditableCurve &curve);
    void handle_viewport_input(EditableCurve &curve, const ClipTransform &transform, bool pole_click);
    void cancel_drag(EditableCurve &curve);
    [[nodiscard]] bool tracking_pole() const
    {
        return tracking_pole_;
    }
    [[nodiscard]] std::size_t selected_index() const
    {
        return static_cast<std::size_t>(settings_.pole_number - 1);
    }

  private:
    void draw_project(EditableCurve &curve);
    void draw_history(EditableCurve &curve);
    void draw_refinement(EditableCurve &curve);
    void draw_point_fitting(EditableCurve &curve);
    void finish_curve_operation(const EditableCurve &curve, const std::expected<void, std::string> &result);
    EditableCurve::Definition draft_;
    occt_lab::document::CurveProjectSettings settings_;
    std::string project_path_ = "curve.occtlab";
    std::string project_status_;
    bool tracking_pole_ = false;
    std::string error_;
};
