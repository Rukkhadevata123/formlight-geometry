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
    EditableCurve::Definition draft_;
    int pole_number_ = 1;
    std::string error_;
};
