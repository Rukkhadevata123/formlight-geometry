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

    [[nodiscard]] auto current() const -> const Curve &;

    // clang-format off
    [[nodiscard]] auto apply(const Definition &definition)       -> std::expected<void, std::string>;
    [[nodiscard]] auto insert_knot(double parameter)             -> std::expected<void, std::string>;
    [[nodiscard]] auto elevate_degree(int target_degree)         -> std::expected<void, std::string>;
    [[nodiscard]] auto interpolate_through_poles()               -> std::expected<void, std::string>;
    [[nodiscard]] auto approximate_data_points(double tolerance) -> std::expected<void, std::string>;
    // clang-format on

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
