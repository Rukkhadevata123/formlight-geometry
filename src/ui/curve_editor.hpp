#pragma once

#include "application/editable_curve.hpp"

class CurveEditor
{
  public:
    explicit CurveEditor(const EditableCurve &curve) : draft_(curve.current().definition)
    {
    }
    void draw(EditableCurve &curve);
    [[nodiscard]] std::size_t selected_index() const
    {
        return static_cast<std::size_t>(pole_number_ - 1);
    }

  private:
    void draw_history(EditableCurve &curve);
    void draw_refinement(EditableCurve &curve);
    void draw_interpolation(EditableCurve &curve);
    void finish_curve_operation(const EditableCurve &curve, const std::expected<void, std::string> &result);
    EditableCurve::Definition draft_;
    int pole_number_ = 1;
    double knot_parameter_ = 0.5;
    std::string error_;
};
