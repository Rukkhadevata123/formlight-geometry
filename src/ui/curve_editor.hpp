#pragma once

#include "application/editable_curve.hpp"

struct ClipTransform;

class CurveEditor
{
  public:
    explicit CurveEditor(const EditableCurve &curve) : draft_(curve.current().definition)
    {
    }
    void draw(EditableCurve &curve);
    void handle_viewport_input(EditableCurve &curve, const ClipTransform &transform);
    void cancel_drag(EditableCurve &curve);
    [[nodiscard]] bool tracking_pole() const
    {
        return tracking_pole_;
    }
    [[nodiscard]] std::size_t selected_index() const
    {
        return static_cast<std::size_t>(pole_number_ - 1);
    }

  private:
    void draw_history(EditableCurve &curve);
    void draw_refinement(EditableCurve &curve);
    void draw_point_fitting(EditableCurve &curve);
    void finish_curve_operation(const EditableCurve &curve, const std::expected<void, std::string> &result);
    EditableCurve::Definition draft_;
    int pole_number_ = 1;
    bool tracking_pole_ = false;
    double knot_parameter_ = 0.5;
    double fit_tolerance_ = 1.0;
    std::string error_;
};
