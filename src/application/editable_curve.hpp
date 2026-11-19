#pragma once

#include "geometry/bspline_curve.hpp"

#include <expected>
#include <optional>
#include <string>

class EditableCurve
{
  public:
    using Curve = occt_lab::geometry::SampledCurve;
    using Definition = occt_lab::geometry::CurveDefinition;
    EditableCurve();

    [[nodiscard]] const Curve &current() const
    {
        return current_;
    }
    [[nodiscard]] std::expected<void, std::string> apply(const Definition &definition);
    [[nodiscard]] std::expected<void, std::string> insert_knot(double parameter);
    [[nodiscard]] std::expected<void, std::string> elevate_degree(int target_degree);
    [[nodiscard]] std::expected<void, std::string> interpolate_through_poles();
    [[nodiscard]] std::expected<void, std::string> approximate_data_points(double tolerance);
    [[nodiscard]] bool history_changes_knots() const
    {
        return history_ && history_->definition.knots != current_.definition.knots;
    }
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
    void commit(Curve candidate);
    void commit_refinement(const Definition &definition);
    Curve current_;
    std::optional<Curve> history_;
    bool undone_ = false;
};
