#pragma once

#include "geometry/bspline_curve.hpp"

#include <expected>
#include <optional>
#include <string>

class EditableCurve
{
  public:
    using Curve = occt_lab::geometry::SampledCurve;
    EditableCurve();

    [[nodiscard]] const Curve &current() const
    {
        return current_;
    }
    [[nodiscard]] std::expected<void, std::string> apply(const Curve::Poles &poles);
    [[nodiscard]] bool can_undo() const
    {
        return history_.has_value() && !undone_;
    }
    [[nodiscard]] bool can_redo() const
    {
        return history_.has_value() && undone_;
    }
    void undo();
    void redo();

  private:
    Curve current_;
    std::optional<Curve> history_;
    bool undone_ = false;
};
